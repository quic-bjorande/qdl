// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2016, Bjorn Andersson <bjorn@kryo.se>
 * All rights reserved.
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>

#include "file.h"
#include "oscompat.h"
#include "qdl.h"
#include "version.h"

#ifdef QDL_TEST_UTIL_MOCK_FS
int qdl_test_mkdir(const char *path);
int qdl_test_stat(const char *path, struct stat *st);
#define qdl_mkdir(path) qdl_test_mkdir(path)
#define qdl_stat(path, st) qdl_test_stat(path, st)
#else
#ifdef _WIN32
#include <direct.h>
#define qdl_mkdir(path) _mkdir(path)
#else
#define qdl_mkdir(path) mkdir(path, 0777)
#endif
#define qdl_stat(path, st) stat(path, st)
#endif

static uint8_t to_hex(uint8_t ch)
{
	ch &= 0xf;
	return ch <= 9 ? '0' + ch : 'a' + ch - 10;
}

void print_version(void)
{
	extern const char *__progname;

	fprintf(stdout, "%s version %s\n", __progname, VERSION);
}

void print_hex_dump(const char *prefix, const void *buf, size_t len)
{
	const uint8_t *ptr = buf;
	size_t linelen;
	uint8_t ch;
	char line[16 * 3 + 16 + 1];
	int li;
	unsigned int i;
	unsigned int j;

	for (i = 0; i < len; i += 16) {
		linelen = MIN(16u, (size_t)(len - i));
		li = 0;

		for (j = 0; j < linelen; j++) {
			ch = ptr[i + j];
			line[li++] = to_hex(ch >> 4);
			line[li++] = to_hex(ch);
			line[li++] = ' ';
		}

		for (; j < 16; j++) {
			line[li++] = ' ';
			line[li++] = ' ';
			line[li++] = ' ';
		}

		for (j = 0; j < linelen; j++) {
			ch = ptr[i + j];
			line[li++] = isprint(ch) ? ch : '.';
		}

		line[li] = '\0';

		printf("%s %04x: %s\n", prefix, i, line);
	}
}

static bool qdl_path_is_sep(char ch)
{
#ifdef _WIN32
	return ch == '/' || ch == '\\';
#else
	return ch == '/';
#endif
}

#ifdef _WIN32
static size_t qdl_unc_root_len(const char *path)
{
	size_t i;
	size_t start;

	if (!qdl_path_is_sep(path[0]) || !qdl_path_is_sep(path[1]))
		return 0;

	i = 2;
	while (qdl_path_is_sep(path[i]))
		i++;
	if (!path[i])
		return 2;

	start = i;
	while (path[i] && !qdl_path_is_sep(path[i]))
		i++;
	if (i == start)
		return 2;

	while (qdl_path_is_sep(path[i]))
		i++;
	if (!path[i])
		return 2;

	start = i;
	while (path[i] && !qdl_path_is_sep(path[i]))
		i++;
	if (i == start)
		return 2;

	return i;
}
#endif

static int qdl_mkdir_one(const char *path)
{
	struct stat st;

	if (qdl_mkdir(path) == 0)
		return 0;

	if (errno != EEXIST)
		return -1;

	if (qdl_stat(path, &st) < 0)
		return -1;

	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}

	return 0;
}

int qdl_ensure_dir(const char *path)
{
	char tmp[PATH_MAX];
	size_t root_len = 0;
	size_t start;
	size_t len;
	size_t i;
	int ret;

	if (!path || path[0] == '\0') {
		errno = EINVAL;
		return -1;
	}

	len = strlen(path);
	if (len >= sizeof(tmp)) {
		errno = ENAMETOOLONG;
		return -1;
	}

	memcpy(tmp, path, len + 1);

#ifdef _WIN32
	if (isalpha((unsigned char)tmp[0]) && tmp[1] == ':')
		root_len = qdl_path_is_sep(tmp[2]) ? 3 : 2;
	else
		root_len = qdl_unc_root_len(tmp);
	if (root_len == 0 && qdl_path_is_sep(tmp[0]))
		root_len = 1;
#else
	if (tmp[0] == '/')
		root_len = 1;
#endif

	while (len > root_len && qdl_path_is_sep(tmp[len - 1]))
		tmp[--len] = '\0';

	if (len == root_len)
		return qdl_mkdir_one(tmp);

	start = root_len;
	if (root_len > 1 && qdl_path_is_sep(tmp[root_len]))
		start++;

	for (i = start; tmp[i]; i++) {
		if (!qdl_path_is_sep(tmp[i]))
			continue;

		tmp[i] = '\0';
		ret = qdl_mkdir_one(tmp);
		tmp[i] = '/';
		if (ret < 0)
			return ret;

		while (qdl_path_is_sep(tmp[i + 1]))
			i++;
	}

	return qdl_mkdir_one(tmp);
}

unsigned int attr_as_unsigned(xmlNode *node, const char *attr, int *errors)
{
	unsigned int ret;
	xmlChar *value;

	value = xmlGetProp(node, (xmlChar *)attr);
	if (!value) {
		(*errors)++;
		return 0;
	}

	ret = (unsigned int)strtoul((char *)value, NULL, 0);
	xmlFree(value);
	return ret;
}

const char *attr_as_string(xmlNode *node, const char *attr, int *errors)
{
	xmlChar *value;
	char *ret = NULL;

	value = xmlGetProp(node, (xmlChar *)attr);
	if (!value) {
		(*errors)++;
		return NULL;
	}

	if (value[0] != '\0')
		ret = strdup((char *)value);

	xmlFree(value);
	return ret;
}

bool attr_as_bool(xmlNode *node, const char *attr, int *errors)
{
	xmlChar *value;
	bool ret = false;

	if (!xmlHasProp(node, (xmlChar *)attr))
		return false;

	value = xmlGetProp(node, (xmlChar *)attr);
	if (!value) {
		(*errors)++;
		return false;
	}

	ret = (xmlStrcmp(value, (xmlChar *)"true") == 0);
	xmlFree(value);
	return ret;
}

static const char * const storage_types[] = {
	[QDL_STORAGE_EMMC] = "emmc",
	[QDL_STORAGE_NAND] = "nand",
	[QDL_STORAGE_NVME] = "nvme",
	[QDL_STORAGE_SPINOR] = "spinor",
	[QDL_STORAGE_UFS] = "ufs",
};

const char *encode_storage_type(enum qdl_storage_type storage)
{
	if ((unsigned int)storage >= ARRAY_SIZE(storage_types))
		return NULL;

	return storage_types[storage];
}

enum qdl_storage_type decode_storage_type(const char *storage)
{
	unsigned int i;

	if (!storage)
		return QDL_STORAGE_UNKNOWN;

	for (i = 0; i < ARRAY_SIZE(storage_types); i++)
		if (storage_types[i] && !strcmp(storage, storage_types[i]))
			return i;

	return QDL_STORAGE_UNKNOWN;
}

/***
 * parse_storage_address() - parse a storage address specifier
 * @address: specifier to be parsed
 * @physical_partition: physical partition
 * @start_sector: start_sector
 * @num_sectors: number of sectors
 * @gpt_partition: GPT name
 *
 * This function parses the provided address specifier and detects the
 * following patterns:
 *
 * N => physical partition N, sector 0
 * N/S => physical partition N, sector S
 * N/S+L => physical partition N, L sectors at sector S
 * name => GPT partition name match across all physical partitions
 * N/name => GPT partition name match within physical partition N
 *
 * @physical_partition is either the requested physical partition, or -1 if
 * none is specified. Either @start_sector and @num_sectors, or @gpt_partition
 * will represent the requested address, the other(s) will be zeroed.
 *
 * Returns: 0 on success, -1 on failure
 */
int parse_storage_address(const char *address, int *physical_partition,
			  unsigned int *start_sector, unsigned int *num_sectors,
			  char **gpt_partition)
{
	unsigned long length = 0;
	const char *ptr = address;
	unsigned long sector = 0;
	long partition;
	char *end;
	char *gpt = NULL;

	errno = 0;
	partition = strtol(ptr, &end, 10);
	if (end == ptr) {
		partition = -1;
		gpt = strdup(ptr);
		goto done;
	}
	if ((errno == ERANGE && partition == LONG_MAX) || partition < 0)
		return -1;

	if (end[0] == '\0')
		goto done;
	if (end[0] != '/')
		return -1;

	ptr = end + 1;

	errno = 0;
	sector = strtoul(ptr, &end, 10);
	if (end == ptr) {
		gpt = strdup(ptr);
		goto done;
	}
	if (errno == ERANGE && sector == ULONG_MAX)
		return -1;

	if (end[0] == '\0')
		goto done;
	if (end[0] != '+')
		return -1;

	ptr = end + 1;

	errno = 0;
	length = strtoul(ptr, &end, 10);
	if (end == ptr)
		return -1;
	if (errno == ERANGE && length == ULONG_MAX)
		return -1;
	if (length == 0)
		return -1;

	if (end[0] != '\0')
		return -1;

done:
	*physical_partition = partition;
	*start_sector = sector;
	*num_sectors = length;
	*gpt_partition = gpt;

	return 0;
}

/**
 * load_sahara_image() - Load the content of the given file into the image
 * @filename: file to be loaded
 * @image: Sahara image object to be populated
 *
 * Read the content of the given @filename into the given @image, update the
 * @image->len, and then populate the @image->name for debugging purposes.
 *
 * Returns: 0 on success, -1 on error
 */
int load_sahara_image(struct qdl_zip *zip, const char *filename, struct sahara_image *image)
{
	struct qdl_file file;
	size_t len;
	void *ptr;
	int ret;

	ret = qdl_file_open(zip, filename, &file);
	if (ret < 0) {
		ux_err("failed to read \"%s\"\n", filename);
		return -1;
	}

	ptr = qdl_file_load(&file, &len);
	if (!ptr || len == 0)
		goto err_close;

	image->name = strdup(filename);
	image->ptr = ptr;
	image->len = len;

	qdl_file_close(&file);

	return 0;

err_close:
	qdl_file_close(&file);
	return -1;
}

void sahara_images_free(struct sahara_image *images, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		free(images[i].name);
		free(images[i].ptr);
		images[i] = (struct sahara_image){};
	}
}
