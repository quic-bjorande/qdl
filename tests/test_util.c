// SPDX-License-Identifier: BSD-3-Clause
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <cmocka.h>

#include "file.h"
#include "qdl.h"

#define MOCK_PATHS_MAX 32

struct mock_fs_entry {
	char path[PATH_MAX];
	bool is_dir;
};

static struct mock_fs_entry mock_entries[MOCK_PATHS_MAX];
static size_t mock_entries_count;
static size_t mock_mkdir_calls_count;

const char *__progname = "test_util";
bool qdl_debug;

static int mock_find_path(const char *path)
{
	size_t i;

	for (i = 0; i < mock_entries_count; i++) {
		if (!strcmp(mock_entries[i].path, path))
			return (int)i;
	}

	return -1;
}

static void mock_add_path(const char *path, bool is_dir)
{
	assert_true(mock_entries_count < ARRAY_SIZE(mock_entries));
	assert_true(strlen(path) < sizeof(mock_entries[0].path));

	strcpy(mock_entries[mock_entries_count].path, path);
	mock_entries[mock_entries_count].is_dir = is_dir;
	mock_entries_count++;
}

int qdl_test_mkdir(const char *path)
{
	int entry;

	assert_true(mock_mkdir_calls_count < MOCK_PATHS_MAX);
	mock_mkdir_calls_count++;

	entry = mock_find_path(path);
	if (entry >= 0) {
		errno = EEXIST;
		return -1;
	}

	mock_add_path(path, true);
	return 0;
}

int qdl_test_stat(const char *path, struct stat *st)
{
	int entry;

	entry = mock_find_path(path);
	if (entry < 0) {
		errno = ENOENT;
		return -1;
	}

	memset(st, 0, sizeof(*st));
	st->st_mode = mock_entries[entry].is_dir ? S_IFDIR : S_IFREG;
	return 0;
}

static int setup_mock_fs(void **state)
{
	(void)state;

	mock_entries_count = 0;
	mock_mkdir_calls_count = 0;
	return 0;
}

static void assert_mock_dir_exists(const char *path)
{
	int entry = mock_find_path(path);

	assert_true(entry >= 0);
	assert_true(mock_entries[entry].is_dir);
}

static void test_ensure_dir_creates_nested_path(void **state)
{
	(void)state;

	assert_int_equal(qdl_ensure_dir("root/one/two"), 0);
	assert_mock_dir_exists("root");
	assert_mock_dir_exists("root/one");
	assert_mock_dir_exists("root/one/two");
	assert_int_equal(mock_mkdir_calls_count, 3);
}

static void test_ensure_dir_accepts_existing_path(void **state)
{
	(void)state;

	mock_add_path("root", true);
	mock_add_path("root/one", true);

	assert_int_equal(qdl_ensure_dir("root/one"), 0);
	assert_int_equal(mock_mkdir_calls_count, 2);
}

static void test_ensure_dir_rejects_file_component(void **state)
{
	(void)state;

	mock_add_path("root", true);
	mock_add_path("root/file", false);

	assert_int_equal(qdl_ensure_dir("root/file/child"), -1);
	assert_int_equal(errno, ENOTDIR);
}

static void test_ensure_dir_rejects_empty_path(void **state)
{
	(void)state;

	assert_int_equal(qdl_ensure_dir(""), -1);
	assert_int_equal(errno, EINVAL);
	assert_int_equal(qdl_ensure_dir(NULL), -1);
	assert_int_equal(errno, EINVAL);
	assert_int_equal(mock_mkdir_calls_count, 0);
}

int qdl_file_open(struct qdl_zip *qzip, const char *filename, struct qdl_file *file)
{
	(void)qzip;
	(void)filename;
	(void)file;
	return -1;
}

void *qdl_file_load(struct qdl_file *file, size_t *len)
{
	(void)file;
	(void)len;
	return NULL;
}

void qdl_file_close(struct qdl_file *file)
{
	(void)file;
}

void ux_err(const char *fmt, ...)
{
	(void)fmt;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_ensure_dir_creates_nested_path, setup_mock_fs),
		cmocka_unit_test_setup(test_ensure_dir_accepts_existing_path, setup_mock_fs),
		cmocka_unit_test_setup(test_ensure_dir_rejects_file_component, setup_mock_fs),
		cmocka_unit_test_setup(test_ensure_dir_rejects_empty_path, setup_mock_fs),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
