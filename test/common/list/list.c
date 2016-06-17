#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "tests.h"
#include "list.h"

static void test_test(void **state) {
	list_t *list = create_list();
	free(list);
	assert_true(true);
}

int main() {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_test),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
