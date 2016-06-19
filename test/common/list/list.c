#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "tests.h"
#include "list.h"

static void test_create_list(void **state) {
	memory_behavior(WRAPPER_INVOKE_CMOCKA);
	list_t *list = create_list();
	assert_int_equal(list->length, 0);
	list_free(list);
}

int main(int argc, char **argv) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_create_list),
	};
	return cmocka_run_group_tests(tests, reset_mem_wrappers, NULL);
}
