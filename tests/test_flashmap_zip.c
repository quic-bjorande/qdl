// SPDX-License-Identifier: BSD-3-Clause
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zip.h>

#ifdef _WIN32
#include <direct.h>
#endif

#include <cmocka.h>

#include "file.h"
#include "firehose.h"
#include "flashmap.h"
#include "oscompat.h"
#include "qdl.h"

#ifdef _WIN32
const char *__progname = "test_zip_flashmap_load";
#endif

static const char flashmap_json[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": ["
	"            {"
	"              \"memory\": \"spinor\","
	"              \"slot\": 0,"
	"              \"files\": [\"rawprogram0.xml\", \"patch0.xml\"]"
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_missing_products[] =
	"{"
	"  \"version\": \"1.1.0\""
	"}";

static const char flashmap_json_empty_products[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": []"
	"}";

static const char flashmap_json_multiple_products[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test-0\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": []"
	"        }"
	"      ]"
	"    },"
	"    {"
	"      \"name\": \"unit-test-1\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout1\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": []"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_missing_layouts[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\""
	"    }"
	"  ]"
	"}";

static const char flashmap_json_empty_layouts[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
		"      \"name\": \"unit-test\","
		"      \"layouts\": []"
		"    }"
		"  ]"
		"}";

static const char flashmap_json_multiple_layouts[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": []"
	"        },"
	"        {"
	"          \"name\": \"layout1\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": []"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_missing_programmer[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmable\": ["
	"            {"
	"              \"memory\": \"spinor\","
	"              \"slot\": 0,"
	"              \"files\": [\"rawprogram0.xml\", \"patch0.xml\"]"
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_missing_programmable[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_empty_programmable[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": []"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_missing_files[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": ["
	"            {"
	"              \"memory\": \"spinor\","
	"              \"slot\": 0"
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_version_type[] =
	"{"
	"  \"version\": 110,"
	"  \"products\": []"
	"}";

static const char flashmap_json_wrong_products_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": {}"
	"}";

static const char flashmap_json_wrong_product_entry_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": [\"bad\"]"
	"}";

static const char flashmap_json_wrong_layouts_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": {}"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_layout_entry_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": [0]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_programmer_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": \"prog.melf\","
	"          \"programmable\": []"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_programmer_entry_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [1],"
	"          \"programmable\": []"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_programmable_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": {}"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_programmable_entry_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": [\"bad\"]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_memory_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": ["
	"            {"
	"              \"memory\": 1,"
	"              \"slot\": 0,"
	"              \"files\": [\"rawprogram0.xml\", \"patch0.xml\"]"
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_slot_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": ["
	"            {"
	"              \"memory\": \"spinor\","
	"              \"slot\": \"0\","
	"              \"files\": [\"rawprogram0.xml\", \"patch0.xml\"]"
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_files_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": ["
	"            {"
	"              \"memory\": \"spinor\","
	"              \"slot\": 0,"
	"              \"files\": \"rawprogram0.xml\""
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char flashmap_json_wrong_file_entry_type[] =
	"{"
	"  \"version\": \"1.1.0\","
	"  \"products\": ["
	"    {"
	"      \"name\": \"unit-test\","
	"      \"layouts\": ["
	"        {"
	"          \"name\": \"layout0\","
	"          \"programmer\": [\"prog.melf\"],"
	"          \"programmable\": ["
	"            {"
	"              \"memory\": \"spinor\","
	"              \"slot\": 0,"
	"              \"files\": [1, \"patch0.xml\"]"
	"            }"
	"          ]"
	"        }"
	"      ]"
	"    }"
	"  ]"
	"}";

static const char rawprogram0_xml[] =
	"<data>"
	"  <program SECTOR_SIZE_IN_BYTES=\"512\" "
	"           filename=\"partition.bin\" "
	"           label=\"xbl\" "
	"           num_partition_sectors=\"1\" "
	"           physical_partition_number=\"0\" "
	"           sparse=\"false\" "
	"           start_sector=\"0\" "
	"           file_sector_offset=\"0\"/>"
	"</data>";

static const char patch0_xml[] =
	"<patches>"
	"  <patch SECTOR_SIZE_IN_BYTES=\"512\" "
	"         byte_offset=\"0\" "
	"         filename=\"DISK\" "
	"         physical_partition_number=\"0\" "
	"         size_in_bytes=\"4\" "
	"         start_sector=\"0\" "
	"         value=\"0x1\" "
	"         what=\"unit\"/>"
	"</patches>";

static const unsigned char programmer_blob[] = { 0x7f, 'E', 'L', 'F', 0x01, 0x02, 0x03, 0x04 };
static const unsigned char partition_blob[] = { 0xde, 0xad, 0xbe, 0xef };

bool qdl_debug;

void ux_init(void)
{
}

void ux_err(const char *fmt __unused, ...)
{
}

void ux_info(const char *fmt __unused, ...)
{
}

void ux_log(const char *fmt __unused, ...)
{
}

void ux_debug(const char *fmt __unused, ...)
{
}

void ux_progress(const char *fmt __unused, unsigned int value __unused, unsigned int size __unused, ...)
{
}

enum qdl_storage_type decode_storage(const char *storage)
{
	if (!strcmp(storage, "emmc"))
		return QDL_STORAGE_EMMC;
	if (!strcmp(storage, "nand"))
		return QDL_STORAGE_NAND;
	if (!strcmp(storage, "nvme"))
		return QDL_STORAGE_NVME;
	if (!strcmp(storage, "spinor"))
		return QDL_STORAGE_SPINOR;
	if (!strcmp(storage, "ufs"))
		return QDL_STORAGE_UFS;

	return QDL_STORAGE_UNKNOWN;
}

struct firehose_op *firehose_alloc_op(int type)
{
	struct firehose_op *op;

	op = calloc(1, sizeof(*op));
	if (!op)
		return NULL;

	op->type = type;
	return op;
}

void firehose_free_ops(struct list_head *ops)
{
	struct firehose_op *next;
	struct firehose_op *op;

	list_for_each_entry_safe(op, next, ops, node) {
		list_del(&op->node);
		qdl_zip_put(op->zip);
		free((void *)op->filename);
		free((void *)op->label);
		free((void *)op->start_sector);
		free((void *)op->gpt_partition);
		free((void *)op->value);
		free((void *)op->what);
		free(op);
	}
}

static int write_file(const char *path, const void *buf, size_t size)
{
	ssize_t ret;
	int fd;

	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644);
	if (fd < 0)
		return -1;

	ret = write(fd, buf, size);
	close(fd);

	if (ret < 0 || (size_t)ret != size)
		return -1;

	return 0;
}

static int make_dir(const char *path)
{
#ifdef _WIN32
	return _mkdir(path);
#else
	return mkdir(path, 0700);
#endif
}

static int join_path(char *path, size_t path_size, const char *dir, const char *name)
{
	int ret;

	ret = snprintf(path, path_size, "%s/%s", dir, name);
	if (ret < 0 || (size_t)ret >= path_size)
		return -1;

	return 0;
}

static int make_temp_dir(char *path, size_t path_size)
{
	const char *root;
	int ret;

	root = getenv("TMPDIR");
	if (!root || !root[0])
		root = getenv("TEMP");
	if (!root || !root[0])
		root = getenv("TMP");
	if (!root || !root[0])
		root = ".";

	for (int i = 0; i < 100; i++) {
		ret = snprintf(path, path_size, "%s/qdl-flashmap-test-%ld-%d",
			       root, (long)getpid(), i);
		if (ret < 0 || (size_t)ret >= path_size)
			return -1;

		if (make_dir(path) == 0)
			return 0;
		if (errno != EEXIST)
			return -1;
	}

	return -1;
}

static int populate_plain_fixture(const char *dir)
{
	char path[PATH_MAX];

	if (join_path(path, sizeof(path), dir, "flashmap.json"))
		return -1;
	if (write_file(path, flashmap_json, strlen(flashmap_json)))
		return -1;

	if (join_path(path, sizeof(path), dir, "rawprogram0.xml"))
		return -1;
	if (write_file(path, rawprogram0_xml, strlen(rawprogram0_xml)))
		return -1;

	if (join_path(path, sizeof(path), dir, "patch0.xml"))
		return -1;
	if (write_file(path, patch0_xml, strlen(patch0_xml)))
		return -1;

	if (join_path(path, sizeof(path), dir, "prog.melf"))
		return -1;
	if (write_file(path, programmer_blob, sizeof(programmer_blob)))
		return -1;

	if (join_path(path, sizeof(path), dir, "partition.bin"))
		return -1;
	if (write_file(path, partition_blob, sizeof(partition_blob)))
		return -1;

	return 0;
}

static int zip_add_buffer(zip_t *zip, const char *name, const void *buf, size_t size)
{
	zip_source_t *source;

	source = zip_source_buffer(zip, buf, size, 0);
	if (!source)
		return -1;

	if (zip_file_add(zip, name, source, ZIP_FL_OVERWRITE) < 0) {
		zip_source_free(source);
		return -1;
	}

	return 0;
}

static int create_fixture_zip(const char *zip_path)
{
	zip_t *zip;
	int err;

	zip = zip_open(zip_path, ZIP_CREATE | ZIP_TRUNCATE, &err);
	if (!zip)
		return -1;

	if (zip_add_buffer(zip, "flashmap.json", flashmap_json, strlen(flashmap_json)) ||
	    zip_add_buffer(zip, "rawprogram0.xml", rawprogram0_xml, strlen(rawprogram0_xml)) ||
	    zip_add_buffer(zip, "patch0.xml", patch0_xml, strlen(patch0_xml)) ||
	    zip_add_buffer(zip, "prog.melf", programmer_blob, sizeof(programmer_blob)) ||
	    zip_add_buffer(zip, "partition.bin", partition_blob, sizeof(partition_blob))) {
		zip_discard(zip);
		return -1;
	}

	if (zip_close(zip) < 0)
		return -1;

	return 0;
}

static bool ends_with(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if (str_len < suffix_len)
		return false;

	return !strcmp(str + str_len - suffix_len, suffix);
}

static int verify_result(struct list_head *ops, struct sahara_image *images, bool expect_zip_ops)
{
	struct firehose_op *op;
	int idx = 0;

	assert_non_null(images[SAHARA_ID_EHOSTDL_IMG].ptr);
	assert_int_equal(images[SAHARA_ID_EHOSTDL_IMG].len, sizeof(programmer_blob));
	assert_non_null(images[SAHARA_ID_EHOSTDL_IMG].name);
	assert_true(ends_with(images[SAHARA_ID_EHOSTDL_IMG].name, "prog.melf"));

	list_for_each_entry(op, ops, node) {
		if (idx == 0) {
			assert_int_equal(op->type, FIREHOSE_OP_CONFIGURE);
			assert_int_equal(op->storage_type, QDL_STORAGE_SPINOR);
		} else if (idx == 1) {
			assert_int_equal(op->type, FIREHOSE_OP_PROGRAM);
			assert_true(op->filename && ends_with(op->filename, "partition.bin"));
			assert_int_equal(!!op->zip, expect_zip_ops);
		} else if (idx == 2) {
			assert_int_equal(op->type, FIREHOSE_OP_PATCH);
			assert_non_null(op->filename);
			assert_string_equal(op->filename, "DISK");
		} else {
			fail_msg("unexpected extra op at index %d", idx);
		}
		idx++;
	}

	assert_int_equal(idx, 3);

	return 0;
}

static int verify_missing_hierarchy_cases(const char *json_path, const char *dir)
{
	struct {
		const char *name;
		const char *json;
	} cases[] = {
		{ "missing products", flashmap_json_missing_products },
		{ "empty products", flashmap_json_empty_products },
		{ "multiple products", flashmap_json_multiple_products },
		{ "missing layouts", flashmap_json_missing_layouts },
		{ "empty layouts", flashmap_json_empty_layouts },
		{ "multiple layouts", flashmap_json_multiple_layouts },
		{ "missing programmer", flashmap_json_missing_programmer },
		{ "missing programmable", flashmap_json_missing_programmable },
		{ "empty programmable", flashmap_json_empty_programmable },
		{ "missing files", flashmap_json_missing_files },
	};
	size_t i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct sahara_image images[MAPPING_SZ] = { 0 };
		struct list_head ops = LIST_INIT(ops);

		ret = write_file(json_path, cases[i].json, strlen(cases[i].json));
		assert_int_equal(ret, 0);

		ret = flashmap_load(&ops, json_path, images, dir);
		assert_true(ret < 0);
		assert_true(list_empty(&ops));
		assert_true(!images[SAHARA_ID_EHOSTDL_IMG].ptr &&
			    !images[SAHARA_ID_EHOSTDL_IMG].name);

		firehose_free_ops(&ops);
		sahara_images_free(images, MAPPING_SZ);
	}

	ret = write_file(json_path, flashmap_json, strlen(flashmap_json));
	assert_int_equal(ret, 0);

	return 0;
}

static int verify_wrong_type_cases(const char *json_path, const char *dir)
{
	struct {
		const char *name;
		const char *json;
	} cases[] = {
		{ "wrong version type", flashmap_json_wrong_version_type },
		{ "wrong products type", flashmap_json_wrong_products_type },
		{ "wrong product entry type", flashmap_json_wrong_product_entry_type },
		{ "wrong layouts type", flashmap_json_wrong_layouts_type },
		{ "wrong layout entry type", flashmap_json_wrong_layout_entry_type },
		{ "wrong programmer type", flashmap_json_wrong_programmer_type },
		{ "wrong programmer entry type", flashmap_json_wrong_programmer_entry_type },
		{ "wrong programmable type", flashmap_json_wrong_programmable_type },
		{ "wrong programmable entry type", flashmap_json_wrong_programmable_entry_type },
		{ "wrong memory type", flashmap_json_wrong_memory_type },
		{ "wrong slot type", flashmap_json_wrong_slot_type },
		{ "wrong files type", flashmap_json_wrong_files_type },
		{ "wrong file entry type", flashmap_json_wrong_file_entry_type },
	};
	size_t i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct sahara_image images[MAPPING_SZ] = { 0 };
		struct list_head ops = LIST_INIT(ops);

		ret = write_file(json_path, cases[i].json, strlen(cases[i].json));
		assert_int_equal(ret, 0);

		ret = flashmap_load(&ops, json_path, images, dir);
		assert_true(ret < 0);
		assert_true(list_empty(&ops));
		assert_true(!images[SAHARA_ID_EHOSTDL_IMG].ptr &&
			    !images[SAHARA_ID_EHOSTDL_IMG].name);

		firehose_free_ops(&ops);
		sahara_images_free(images, MAPPING_SZ);
	}

	ret = write_file(json_path, flashmap_json, strlen(flashmap_json));
	assert_int_equal(ret, 0);

	return 0;
}

static void cleanup_fixture(const char *dir)
{
	char path[PATH_MAX];
	static const char * const files[] = {
		"flashmap.json",
		"rawprogram0.xml",
		"patch0.xml",
		"prog.melf",
		"partition.bin",
		"install-mini.zip",
	};

	for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
		if (!join_path(path, sizeof(path), dir, files[i]))
			unlink(path);
	}

	rmdir(dir);
}

typedef int (*plain_fixture_case_fn)(const char *json_path, const char *dir);
typedef int (*temp_dir_case_fn)(const char *dir);

static int run_with_plain_fixture(plain_fixture_case_fn fn)
{
	char json_path[PATH_MAX];
	char dir[PATH_MAX];
	int ret;

	ret = make_temp_dir(dir, sizeof(dir));
	assert_int_equal(ret, 0);

	ret = populate_plain_fixture(dir);
	if (ret != 0) {
		cleanup_fixture(dir);
		assert_int_equal(ret, 0);
	}

	ret = join_path(json_path, sizeof(json_path), dir, "flashmap.json");
	if (ret != 0) {
		cleanup_fixture(dir);
		assert_int_equal(ret, 0);
	}

	ret = fn(json_path, dir);
	cleanup_fixture(dir);
	return ret;
}

static int run_with_temp_dir(temp_dir_case_fn fn)
{
	char dir[PATH_MAX];
	int ret;

	ret = make_temp_dir(dir, sizeof(dir));
	assert_int_equal(ret, 0);

	ret = fn(dir);
	cleanup_fixture(dir);
	return ret;
}

static int case_plain_flashmap_load(const char *json_path, const char *dir)
{
	struct sahara_image images[MAPPING_SZ] = { 0 };
	struct list_head ops = LIST_INIT(ops);
	int ret;

	ret = flashmap_load(&ops, json_path, images, dir);
	assert_int_equal(ret, 0);
	assert_int_equal(verify_result(&ops, images, false), 0);

	firehose_free_ops(&ops);
	sahara_images_free(images, MAPPING_SZ);
	return 0;
}

static int case_missing_hierarchy(const char *json_path, const char *dir)
{
	return verify_missing_hierarchy_cases(json_path, dir);
}

static int case_wrong_type(const char *json_path, const char *dir)
{
	return verify_wrong_type_cases(json_path, dir);
}

static int case_zip_flashmap_load(const char *dir)
{
	struct sahara_image images[MAPPING_SZ] = { 0 };
	struct list_head ops = LIST_INIT(ops);
	char zip_path[PATH_MAX];
	int ret;

	ret = join_path(zip_path, sizeof(zip_path), dir, "install-mini.zip");
	assert_int_equal(ret, 0);

	ret = create_fixture_zip(zip_path);
	assert_int_equal(ret, 0);

	ret = flashmap_load(&ops, zip_path, images, dir);
	assert_int_equal(ret, 0);
	assert_int_equal(verify_result(&ops, images, true), 0);

	firehose_free_ops(&ops);
	sahara_images_free(images, MAPPING_SZ);
	return 0;
}

static void test_plain_flashmap_load(void **state)
{
	(void)state;

	assert_int_equal(run_with_plain_fixture(case_plain_flashmap_load), 0);
}

static void test_missing_hierarchy(void **state)
{
	(void)state;

	assert_int_equal(run_with_plain_fixture(case_missing_hierarchy), 0);
}

static void test_wrong_type(void **state)
{
	(void)state;

	assert_int_equal(run_with_plain_fixture(case_wrong_type), 0);
}

static void test_zip_flashmap_load(void **state)
{
	(void)state;

	assert_int_equal(run_with_temp_dir(case_zip_flashmap_load), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_plain_flashmap_load),
		cmocka_unit_test(test_missing_hierarchy),
		cmocka_unit_test(test_wrong_type),
		cmocka_unit_test(test_zip_flashmap_load),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
