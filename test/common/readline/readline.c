#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include "tests.h"
#include "readline.h"

int __wrap_fgetc(FILE *stream) {
	return mock_type(int);
}

static void prep_string(const char *str) {
	while (*str) {
		will_return(__wrap_fgetc, *str++);
	}
}

static void test_eof_line_ending(void **state) {
	prep_string("hello");
	will_return(__wrap_fgetc, EOF);
	char *line = read_line(NULL);
	assert_string_equal(line, "hello");
	free(line);
}

static void test_newline(void **state) {
	prep_string("hello\n");
	char *line = read_line(NULL);
	assert_string_equal(line, "hello");
	free(line);
}

static void test_continuation(void **state) {
	prep_string("hello \\\nworld");
	will_return(__wrap_fgetc, EOF);
	char *line = read_line(NULL);
	assert_string_equal(line, "hello world");
	free(line);
}

static void test_expand_buffer(void **state) {
	const char *test = "This is a very very long string. "
		"This string is so long that it may in fact be greater "
		"than 128 bytes (or octets) in length, which is suitable "
		"for triggering a realloc";
	prep_string(test);
	will_return(__wrap_fgetc, EOF);
	char *line = read_line(NULL);
	assert_string_equal(line, test);
	free(line);
	assert_int_equal(realloc_calls(), 1);
}

int main(int argc, char **argv) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_eof_line_ending),
		cmocka_unit_test(test_newline),
		cmocka_unit_test(test_continuation),
		cmocka_unit_test(test_expand_buffer),
	};
	return cmocka_run_group_tests(tests, reset_mem_wrappers, NULL);
}
