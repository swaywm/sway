#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include "tests.h"
#include "list.h"

static void assert_list_contents(list_t *list, int contents[], size_t len) {
	assert_int_equal(list->length, (int)len);
	for (size_t i = 0; i < (size_t)list->length; ++i) {
		assert_int_equal(contents[i], *(int *)list->items[i]);
	}
}

static list_t *create_test_list(int contents[], size_t len) {
	list_t *l = create_list();
	for (size_t i = 0; i < len; ++i) {
		list_add(l, &contents[i]);
	}
	return l;
}

static void test_create_and_free(void **state) {
	list_t *list = create_list();
	assert_int_equal(list->length, 0);
	assert_int_equal(list->capacity, 10);
	assert_non_null(list->items);
	list_free(list);
	assert_int_equal(malloc_calls(), 2);
	assert_int_equal(free_calls(), 2);
}

static void test_add(void **state) {
	list_t *list = create_list();

	int items[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	for (size_t i = 0; i < sizeof(items) / sizeof(int); ++i) {
		list_add(list, &items[i]);
		assert_int_equal(items[i], *(int *)list->items[i]);
		assert_int_equal(list->length, i + 1);
	}
	assert_list_contents(list, items, 15);
	assert_int_equal(list->length, 15);
	assert_int_equal(list->capacity, 20);
	assert_int_equal(realloc_calls(), 1);

	list_free(list);
}

static void test_insert(void **state) {
	list_t *list = create_list();
	int i = 1, j = 2;
	list_add(list, &i);
	list_add(list, &i);
	list_add(list, &i);
	list_insert(list, 0, &j);
	assert_int_equal(j, *(int *)list->items[0]);
	assert_int_equal(list->length, 4);
	list_free(list);
}

static void test_del(void **state) {
	list_t *list = create_list();

	int items[] = { 1, 2, 3, 4, 5 };
	int new_items[] = { 1, 2, 4, 5 };
	for (size_t i = 0; i < sizeof(items) / sizeof(int); ++i) {
		list_add(list, &items[i]);
	}

	list_del(list, 2);
	assert_list_contents(list, new_items, 4);
	list_free(list);
}

static void test_cat(void **state) {
	int items_a[] = { 1, 2, 3, 4 };
	int items_b[] = { 5, 6, 7, 8 };
	int items_final[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	list_t *list_a = create_test_list(items_a, 4);
	list_t *list_b = create_test_list(items_b, 4);

	list_cat(list_a, list_b);

	assert_list_contents(list_a, items_final, 8);
	list_free(list_a);
	list_free(list_b);
}

static int qsort_compare(const void *left, const void *right) {
	return **(int * const *)left - **(int * const *)right;
}

static void test_qsort(void **state) {
	int items_start[] = { 1, 4, 3, 2 };
	int items_final[] = { 1, 2, 3, 4 };
	list_t *list = create_test_list(items_start, 4);
	list_qsort(list, qsort_compare);
	assert_list_contents(list, items_final, 4);
	list_free(list);
}

static int find_compare(const void *a, const void *b) {
	return *(int *)a - *(int *)b;
}

static void test_seq_find(void **state) {
	int items[] = { 1, 2, 3, 4 };
	int expected = 3;
	list_t *list = create_test_list(items, 4);
	int index = list_seq_find(list, find_compare, &expected);
	assert_int_equal(index, 2);
	list_free(list);
}

int foreach_count = 0;

static void foreach(void *item) {
	foreach_count++;
	assert_int_equal(*(int *)item, foreach_count);
}

static void test_foreach(void **state) {
	int items[] = { 1, 2, 3, 4 };
	list_t *list = create_test_list(items, 4);
	list_foreach(list, foreach);
	assert_int_equal(foreach_count, 4);
	list_free(list);
}

struct stable_data {
	int id, value;
};

static int stable_compare(const void *_a, const void *_b) {
	struct stable_data * const *a = _a;
	struct stable_data * const *b = _b;
	return (*a)->value - (*b)->value;
}

static void test_stable_sort(void **state) {
	struct stable_data initial[] = {
		{ .id = 0, .value = 0 },
		{ .id = 3, .value = 2 },
		{ .id = 4, .value = 3 },
		{ .id = 1, .value = 1 },
		{ .id = 2, .value = 1 },
		{ .id = 5, .value = 4 },
		{ .id = 7, .value = 5 },
		{ .id = 6, .value = 5 },
	};
	struct stable_data expected[] = {
		{ .id = 0, .value = 0 },
		{ .id = 1, .value = 1 },
		{ .id = 2, .value = 1 },
		{ .id = 3, .value = 2 },
		{ .id = 4, .value = 3 },
		{ .id = 5, .value = 4 },
		{ .id = 7, .value = 5 },
		{ .id = 6, .value = 5 },
	};
	list_t *list = create_list();
	for (size_t i = 0; i < sizeof(initial) / sizeof(initial[0]); ++i) {
		list_add(list, &initial[i]);
	}
	list_stable_sort(list, stable_compare);
	for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
		struct stable_data *item = list->items[i];
		assert_int_equal(item->value, expected[i].value);
		assert_int_equal(item->id, expected[i].id);
	}
	list_free(list);
}

int main(int argc, char **argv) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_create_and_free),
		cmocka_unit_test(test_add),
		cmocka_unit_test(test_insert),
		cmocka_unit_test(test_del),
		cmocka_unit_test(test_cat),
		cmocka_unit_test(test_qsort),
		cmocka_unit_test(test_seq_find),
		cmocka_unit_test(test_foreach),
		cmocka_unit_test(test_stable_sort),
	};
	return cmocka_run_group_tests(tests, reset_mem_wrappers, NULL);
}
