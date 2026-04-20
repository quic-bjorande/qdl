// SPDX-License-Identifier: BSD-3-Clause
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "json.h"

#ifdef _WIN32
const char *__progname = "test_json";
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

static const char fixture_json[] =
	"{\n"
	"  \"obj\": {\n"
	"    \"inner\": \"inside\"\n"
	"  },\n"
	"  \"arr\": [\n"
	"    \"alpha\",\n"
	"    { \"kind\": \"object\" },\n"
	"    7,\n"
	"    true,\n"
	"    false,\n"
	"    null\n"
	"  ],\n"
	"  \"num\": 42,\n"
	"  \"str\": \"hello\",\n"
	"  \"t\": true,\n"
	"  \"f\": false,\n"
	"  \"n\": null\n"
	"}\n";

static void test_parse_value_types_positive(void **state)
{
	(void)state;
	struct {
		const char *json;
		int type;
	} cases[] = {
		{ "{\"k\":\"v\"}", JSON_TYPE_OBJECT },
		{ "{}", JSON_TYPE_OBJECT },
		{ "[1]", JSON_TYPE_ARRAY },
		{ "[]", JSON_TYPE_ARRAY },
		{ "\"v\"", JSON_TYPE_STRING },
		{ "123", JSON_TYPE_NUMBER },
		{ "true", JSON_TYPE_TRUE },
		{ "false", JSON_TYPE_FALSE },
		{ "null", JSON_TYPE_NULL },
		{ " \t\r\n {\"k\":1} \n", JSON_TYPE_OBJECT },
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct json_value *root;

		root = json_parse_buf(cases[i].json, strlen(cases[i].json));
		assert_true(root);
		assert_true(root->type == cases[i].type);
		assert_true(json_error[0] == '\0');
		json_free(root);
	}

}

static void test_parse_value_types_negative(void **state)
{
	(void)state;
	static const char * const cases[] = {
		"{\"k\"\"v\"}",
		"{\"a\" 1}",
		"{\"a\":}",
		"{\"a\":1 \"b\":2}",
		"{\"a\",1}",
		"[1 2]",
		"[1,2 3]",
		"[,1]",
		"\"unterminated",
		"--1",
		"tru",
		"fals",
		"nul",
		"truex",
		"{\"k\":1,}",
		"[1,]",
		"{\"k\":1}junk",
		"[1] 2",
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct json_value *root;

		root = json_parse_buf(cases[i], strlen(cases[i]));
		assert_true(!root);
		assert_true(json_error[0] != '\0');
	}

}

static void test_error_buffer_reset_and_content(void **state)
{
	(void)state;
	struct json_value *root;

	root = json_parse_buf("{\"a\":}", strlen("{\"a\":}"));
	assert_true(!root);
	assert_true(json_error[0] != '\0');

	root = json_parse_buf("{}", strlen("{}"));
	assert_true(root);
	assert_true(json_error[0] == '\0');
	json_free(root);

}

static void test_parse_buf_len_behavior(void **state)
{
	(void)state;
	static const char input[] = "truejunk";
	struct json_value *root;

	root = json_parse_buf(input, 4);
	assert_true(root);
	assert_true(root->type == JSON_TYPE_TRUE);
	json_free(root);

	root = json_parse_buf(input, 5);
	assert_true(!root);

}

static void test_accessors_positive(void **state)
{
	(void)state;
	struct json_value *root;
	struct json_value *obj;
	struct json_value *arr;
	struct json_value *v;
	const char *s;
	double number = 0;

	root = json_parse_buf(fixture_json, strlen(fixture_json));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_OBJECT);

	obj = json_get_child(root, "obj");
	assert_true(obj);
	assert_true(obj->type == JSON_TYPE_OBJECT);

	s = json_get_string(obj, "inner");
	assert_true(s);
	assert_true(!strcmp(s, "inside"));

	arr = json_get_child(root, "arr");
	assert_true(arr);
	assert_true(arr->type == JSON_TYPE_ARRAY);
	assert_true(json_count_children(arr) == 6);

	s = json_get_element_string(arr, 0);
	assert_true(s);
	assert_true(!strcmp(s, "alpha"));

	v = json_get_element_object(arr, 1);
	assert_true(v);
	assert_true(v->type == JSON_TYPE_OBJECT);
	s = json_get_string(v, "kind");
	assert_true(s);
	assert_true(!strcmp(s, "object"));

	v = json_get_element_object(arr, 2);
	assert_true(v);
	assert_true(v->type == JSON_TYPE_NUMBER);
	assert_true(v->u.number == 7);

	v = json_get_element_object(arr, 3);
	assert_true(v);
	assert_true(v->type == JSON_TYPE_TRUE);

	v = json_get_element_object(arr, 4);
	assert_true(v);
	assert_true(v->type == JSON_TYPE_FALSE);

	v = json_get_element_object(arr, 5);
	assert_true(v);
	assert_true(v->type == JSON_TYPE_NULL);

	assert_true(json_get_number(root, "num", &number) == 0);
	assert_true(number == 42);

	s = json_get_string(root, "str");
	assert_true(s);
	assert_true(!strcmp(s, "hello"));

	v = json_get_child(root, "t");
	assert_true(v);
	assert_true(v->type == JSON_TYPE_TRUE);

	v = json_get_child(root, "f");
	assert_true(v);
	assert_true(v->type == JSON_TYPE_FALSE);

	v = json_get_child(root, "n");
	assert_true(v);
	assert_true(v->type == JSON_TYPE_NULL);

	json_free(root);
}

static void test_accessors_negative(void **state)
{
	(void)state;
	struct json_value *root;
	struct json_value *arr;
	struct json_value *num;
	double number = 0;

	root = json_parse_buf(fixture_json, strlen(fixture_json));
	assert_true(root);

	assert_true(!json_get_child(root, "missing"));

	num = json_get_child(root, "num");
	assert_true(num);
	assert_true(!json_get_child(num, "nested"));

	arr = json_get_child(root, "arr");
	assert_true(arr);
	assert_true(!json_get_element_object(arr, 100));
	assert_true(!json_get_element_object(root, 0));
	assert_true(!json_get_element_string(arr, 1));
	assert_true(!json_get_element_string(root, 0));
	assert_true(!json_get_element_string(NULL, 0));

	assert_true(json_count_children(root) == -1);
	assert_true(json_count_children(NULL) == -1);

	assert_true(json_get_number(root, "str", &number) == -1);
	assert_true(json_get_number(root, "missing", &number) == -1);
	assert_true(json_get_number(arr, "num", &number) == -1);

	assert_true(!json_get_string(root, "num"));
	assert_true(!json_get_string(root, "missing"));
	assert_true(!json_get_string(arr, "str"));

	json_free(root);
}

static void test_empty_container_accessors(void **state)
{
	(void)state;
	struct json_value *root;
	struct json_value *arr;

	root = json_parse_buf("{}", 2);
	assert_true(root);
	assert_true(root->type == JSON_TYPE_OBJECT);
	assert_true(!json_get_child(root, "x"));
	json_free(root);

	arr = json_parse_buf("[]", 2);
	assert_true(arr);
	assert_true(arr->type == JSON_TYPE_ARRAY);
	assert_true(json_count_children(arr) == 0);
	assert_true(!json_get_element_object(arr, 0));
	assert_true(!json_get_element_string(arr, 0));
	json_free(arr);

}

static void test_duplicate_keys_behavior(void **state)
{
	(void)state;
	const char *dup_json =
		"{"
		"\"dup_str\":\"first\","
		"\"dup_str\":\"second\","
		"\"dup_num\":1,"
		"\"dup_num\":2,"
		"\"mixed\":true,"
		"\"mixed\":\"later\""
		"}";
	struct json_value *root;
	struct json_value *node;
	double number = 0;
	const char *str;

	root = json_parse_buf(dup_json, strlen(dup_json));
	assert_true(root);

	node = json_get_child(root, "dup_str");
	assert_true(node);
	assert_true(node->type == JSON_TYPE_STRING);
	assert_true(!strcmp(node->u.string, "first"));
	str = json_get_string(root, "dup_str");
	assert_true(str && !strcmp(str, "first"));

	node = json_get_child(root, "dup_num");
	assert_true(node);
	assert_true(node->type == JSON_TYPE_NUMBER);
	assert_true(node->u.number == 1);
	assert_true(json_get_number(root, "dup_num", &number) == 0);
	assert_true(number == 1);

	node = json_get_child(root, "mixed");
	assert_true(node);
	assert_true(node->type == JSON_TYPE_TRUE);
	assert_true(!json_get_string(root, "mixed"));

	json_free(root);
}

static void test_long_string_values(void **state)
{
	(void)state;
	size_t i;
	size_t len = 512;
	char *json;
	char *p;
	struct json_value *root;
	const char *s;

	json = malloc(len + 3);
	assert_true(json);

	p = json;
	*p++ = '"';
	memset(p, 'a', len);
	p += len;
	*p++ = '"';
	*p = '\0';

	root = json_parse_buf(json, len + 2);
	assert_true(root);
	assert_true(root->type == JSON_TYPE_STRING);
	assert_true(strlen(root->u.string) == len);
	for (i = 0; i < len; i++)
		assert_true(root->u.string[i] == 'a');
	json_free(root);
	free(json);

	len = 400;
	json = malloc(len + 16);
	assert_true(json);

	p = json;
	*p++ = '{';
	*p++ = '"';
	*p++ = 'k';
	*p++ = '"';
	*p++ = ':';
	*p++ = '"';
	memset(p, 'b', len);
	p += len;
	*p++ = '"';
	*p++ = '}';
	*p = '\0';

	root = json_parse_buf(json, (size_t)(p - json));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_OBJECT);
	s = json_get_string(root, "k");
	assert_true(s);
	assert_true(strlen(s) == len);
	for (i = 0; i < len; i++)
		assert_true(s[i] == 'b');
	json_free(root);
	free(json);

}

static void test_long_number_values(void **state)
{
	(void)state;
	const char *long_num = "1234567890123456789012345678901234567890123456789012345678901234567890";
	const char *sci_num = "-1.234567890123456789e42";
	struct json_value *root;
	struct json_value *node;
	double number = 0;

	root = json_parse_buf(long_num, strlen(long_num));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_NUMBER);
	assert_true(root->u.number > 0);
	json_free(root);

	root = json_parse_buf(sci_num, strlen(sci_num));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_NUMBER);
	assert_true(root->u.number < 0);
	json_free(root);

	root = json_parse_buf("{\"n\":1234567890123456789012345678901234567890}",
			      strlen("{\"n\":1234567890123456789012345678901234567890}"));
	assert_true(root);
	assert_true(json_get_number(root, "n", &number) == 0);
	assert_true(number > 0);
	node = json_get_child(root, "n");
	assert_true(node && node->type == JSON_TYPE_NUMBER);
	json_free(root);

	root = json_parse_buf("1e9999", strlen("1e9999"));
	assert_true(!root);

}

static void test_string_escapes_and_unicode(void **state)
{
	(void)state;
	struct json_value *root;
	const unsigned char *bytes;
	const char *s;

	root = json_parse_buf("\"\\n\"", strlen("\"\\n\""));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_STRING);
	assert_true(root->u.string[0] == '\n' && root->u.string[1] == '\0');
	json_free(root);

	root = json_parse_buf("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"",
			      strlen("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\""));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_STRING);
	s = root->u.string;
	assert_true(s[0] == '"' && s[1] == '\\' && s[2] == '/' &&
		    s[3] == '\b' && s[4] == '\f' && s[5] == '\n' &&
		    s[6] == '\r' && s[7] == '\t' && s[8] == '\0');
	json_free(root);

	root = json_parse_buf("\"\\u0041\"", strlen("\"\\u0041\""));
	assert_true(root);
	assert_true(root->type == JSON_TYPE_STRING);
	assert_true(!strcmp(root->u.string, "A"));
	json_free(root);

	root = json_parse_buf("\"\\u03A9\"", strlen("\"\\u03A9\""));
	assert_true(root);
	bytes = (const unsigned char *)root->u.string;
	assert_true(bytes[0] == 0xce && bytes[1] == 0xa9 && bytes[2] == '\0');
	json_free(root);

	root = json_parse_buf("\"\\uD83D\\uDE00\"", strlen("\"\\uD83D\\uDE00\""));
	assert_true(root);
	bytes = (const unsigned char *)root->u.string;
	assert_true(bytes[0] == 0xf0 && bytes[1] == 0x9f &&
		    bytes[2] == 0x98 && bytes[3] == 0x80 &&
		    bytes[4] == '\0');
	json_free(root);

}

static void test_string_escape_and_control_failures(void **state)
{
	(void)state;
	static const char trailing_backslash[] = { '"', 'a', 'b', 'c', '\\' };
	static const char nul_in_string[] = { '"', 'a', '\0', 'b', '"' };
	const char *newline_in_string = "\"a\nb\"";
	const char *tab_in_string = "\"a\tb\"";
	struct json_value *root;

	root = json_parse_buf("\"\\x\"", strlen("\"\\x\""));
	assert_true(!root);
	assert_true(strstr(json_error, "invalid escape"));

	root = json_parse_buf("\"\\u12\"", strlen("\"\\u12\""));
	assert_true(!root);
	assert_true(strstr(json_error, "invalid unicode escape"));

	root = json_parse_buf("\"\\uD800\"", strlen("\"\\uD800\""));
	assert_true(!root);
	assert_true(strstr(json_error, "low surrogate"));

	root = json_parse_buf("\"\\uDC00\"", strlen("\"\\uDC00\""));
	assert_true(!root);
	assert_true(strstr(json_error, "unexpected low surrogate"));

	root = json_parse_buf(trailing_backslash, sizeof(trailing_backslash));
	assert_true(!root);
	assert_true(strstr(json_error, "unterminated escape"));

	root = json_parse_buf(newline_in_string, strlen(newline_in_string));
	assert_true(!root);
	assert_true(strstr(json_error, "control character"));

	root = json_parse_buf(tab_in_string, strlen(tab_in_string));
	assert_true(!root);
	assert_true(strstr(json_error, "control character"));

	root = json_parse_buf(nul_in_string, sizeof(nul_in_string));
	assert_true(!root);
	assert_true(strstr(json_error, "control character"));

}

static void test_number_grammar_edges(void **state)
{
	(void)state;
	struct {
		const char *json;
		double expected;
	} valid[] = {
		{ "0", 0.0 },
		{ "-0", -0.0 },
		{ "10", 10.0 },
		{ "-1.2e-3", -0.0012 },
		{ "1E+2", 100.0 },
		{ "0.0", 0.0 },
	};
	static const char * const invalid[] = {
		"01",
		".1",
		"1.",
		"1e",
		"+1",
		"-",
		"00",
		"-01",
		"1e+",
		"1e-",
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(valid); i++) {
		struct json_value *root;
		double diff;

		root = json_parse_buf(valid[i].json, strlen(valid[i].json));
		assert_true(root);
		assert_true(root->type == JSON_TYPE_NUMBER);
		diff = root->u.number - valid[i].expected;
		if (diff < 0)
			diff = -diff;
		assert_true(diff < 1e-12);
		json_free(root);
	}

	for (i = 0; i < ARRAY_SIZE(invalid); i++) {
		struct json_value *root;

		root = json_parse_buf(invalid[i], strlen(invalid[i]));
		assert_true(!root);
		assert_true(json_error[0] != '\0');
	}

}

static void test_parse_buf_embedded_nul_behavior(void **state)
{
	(void)state;
	static const char true_with_tail[] = { 't', 'r', 'u', 'e', '\0', 'x' };
	static const char object_with_nul[] = { '{', '"', 'a', '"', ':', '1', '}', '\0', 'x' };
	struct json_value *root;

	root = json_parse_buf(true_with_tail, 4);
	assert_true(root);
	assert_true(root->type == JSON_TYPE_TRUE);
	assert_true(json_error[0] == '\0');
	json_free(root);

	root = json_parse_buf(true_with_tail, sizeof(true_with_tail));
	assert_true(!root);
	assert_true(strstr(json_error, "trailing token") ||
		    strstr(json_error, "unable to match value"));

	root = json_parse_buf(object_with_nul, sizeof(object_with_nul));
	assert_true(!root);
	assert_true(strstr(json_error, "trailing token"));

}

static char *build_nested_array_json(size_t depth)
{
	char *buf;
	size_t i;
	size_t len = depth * 2 + 2;

	buf = malloc(len);
	if (!buf)
		return NULL;

	for (i = 0; i < depth; i++)
		buf[i] = '[';
	buf[depth] = '0';
	for (i = 0; i < depth; i++)
		buf[depth + 1 + i] = ']';
	buf[len - 1] = '\0';

	return buf;
}

static char *build_nested_object_json(size_t depth)
{
	char *buf;
	size_t i;
	size_t pos = 0;
	size_t len = depth * 6 + 2;

	buf = malloc(len);
	if (!buf)
		return NULL;

	for (i = 0; i < depth; i++) {
		memcpy(buf + pos, "{\"a\":", 5);
		pos += 5;
	}
	buf[pos++] = '0';
	for (i = 0; i < depth; i++)
		buf[pos++] = '}';
	buf[pos] = '\0';

	return buf;
}

static void test_nesting_depth_limit(void **state)
{
	(void)state;
	char *json;
	struct json_value *root;

	json = build_nested_array_json(JSON_MAX_DEPTH);
	assert_true(json);
	root = json_parse_buf(json, strlen(json));
	assert_true(root);
	json_free(root);
	free(json);

	json = build_nested_array_json(JSON_MAX_DEPTH + 1);
	assert_true(json);
	root = json_parse_buf(json, strlen(json));
	assert_true(!root);
	assert_true(strstr(json_error, "maximum nesting depth"));
	free(json);

	json = build_nested_object_json(JSON_MAX_DEPTH);
	assert_true(json);
	root = json_parse_buf(json, strlen(json));
	assert_true(root);
	json_free(root);
	free(json);

	json = build_nested_object_json(JSON_MAX_DEPTH + 1);
	assert_true(json);
	root = json_parse_buf(json, strlen(json));
	assert_true(!root);
	assert_true(strstr(json_error, "maximum nesting depth"));
	free(json);

}

static void test_json_error_first_message_specificity(void **state)
{
	(void)state;
	struct json_value *root;

	root = json_parse_buf("{\"a\":}", strlen("{\"a\":}"));
	assert_true(!root);
	assert_true(strstr(json_error, "unable to match value"));

	root = json_parse_buf("{\"a\" 1}", strlen("{\"a\" 1}"));
	assert_true(!root);
	assert_true(strstr(json_error, "expected ':'"));

	root = json_parse_buf("{\"a\":1 \"b\":2}", strlen("{\"a\":1 \"b\":2}"));
	assert_true(!root);
	assert_true(strstr(json_error, "expected ',' or '}'"));

}

static void test_json_free_null(void **state)
{
	(void)state;
	json_free(NULL);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_parse_value_types_positive),
		cmocka_unit_test(test_parse_value_types_negative),
		cmocka_unit_test(test_parse_buf_len_behavior),
		cmocka_unit_test(test_error_buffer_reset_and_content),
		cmocka_unit_test(test_json_error_first_message_specificity),
		cmocka_unit_test(test_string_escapes_and_unicode),
		cmocka_unit_test(test_string_escape_and_control_failures),
		cmocka_unit_test(test_number_grammar_edges),
		cmocka_unit_test(test_parse_buf_embedded_nul_behavior),
		cmocka_unit_test(test_nesting_depth_limit),
		cmocka_unit_test(test_accessors_positive),
		cmocka_unit_test(test_accessors_negative),
		cmocka_unit_test(test_empty_container_accessors),
		cmocka_unit_test(test_duplicate_keys_behavior),
		cmocka_unit_test(test_long_string_values),
		cmocka_unit_test(test_long_number_values),
		cmocka_unit_test(test_json_free_null),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
