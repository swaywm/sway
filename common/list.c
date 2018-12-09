#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

list_t *create_list(void) {
	list_t *list = malloc(sizeof(list_t));
	if (!list) {
		return NULL;
	}
	list->capacity = 10;
	list->length = 0;
	list->items = malloc(sizeof(void*) * list->capacity);
	return list;
}

static void list_resize(list_t *list) {
	if (list->length == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, sizeof(void*) * list->capacity);
	}
}

void list_free(list_t *list) {
	if (list == NULL) {
		return;
	}
	free(list->items);
	free(list);
}

void list_add(list_t *list, void *item) {
	list_resize(list);
	list->items[list->length++] = item;
}

void list_insert(list_t *list, int index, void *item) {
	list_resize(list);
	memmove(&list->items[index + 1], &list->items[index], sizeof(void*) * (list->length - index));
	list->length++;
	list->items[index] = item;
}

void list_del(list_t *list, int index) {
	list->length--;
	memmove(&list->items[index], &list->items[index + 1], sizeof(void*) * (list->length - index));
}

void list_cat(list_t *list, list_t *source) {
	for (int i = 0; i < source->length; ++i) {
		list_add(list, source->items[i]);
	}
}

void list_qsort(list_t *list, int compare(const void *left, const void *right)) {
	qsort(list->items, list->length, sizeof(void *), compare);
}

int list_seq_find(list_t *list, int compare(const void *item, const void *data), const void *data) {
	for (int i = 0; i < list->length; i++) {
		void *item = list->items[i];
		if (compare(item, data) == 0) {
			return i;
		}
	}
	return -1;
}

int list_find(list_t *list, const void *item) {
	for (int i = 0; i < list->length; i++) {
		if (list->items[i] == item) {
			return i;
		}
	}
	return -1;
}

void list_swap(list_t *list, int src, int dest) {
	void *tmp = list->items[src];
	list->items[src] = list->items[dest];
	list->items[dest] = tmp;
}

void list_move_to_end(list_t *list, void *item) {
	int i;
	for (i = 0; i < list->length; ++i) {
		if (list->items[i] == item) {
			break;
		}
	}
	if (!sway_assert(i < list->length, "Item not found in list")) {
		return;
	}
	list_del(list, i);
	list_add(list, item);
}

static void list_rotate(list_t *list, int from, int to) {
	void *tmp = list->items[to];

	while (to > from) {
		list->items[to] = list->items[to - 1];
		to--;
	}

	list->items[from] = tmp;
}

static void list_inplace_merge(list_t *list, int left, int last, int mid, int compare(const void *a, const void *b)) {
	int right = mid + 1;

	if (compare(&list->items[mid], &list->items[right]) <= 0) {
		return;
	}

	while (left <= mid && right <= last) {
		if (compare(&list->items[left], &list->items[right]) <= 0) {
			left++;
		} else {
			list_rotate(list, left, right);
			left++;
			mid++;
			right++;
		}
	}
}

static void list_inplace_sort(list_t *list, int first, int last, int compare(const void *a, const void *b)) {
	if (first >= last) {
		return;
	} else if ((last - first) == 1) {
		if (compare(&list->items[first], &list->items[last]) > 0) {
			list_swap(list, first, last);
		}
	} else {
		int mid = (int)((last + first) / 2);
		list_inplace_sort(list, first, mid, compare);
		list_inplace_sort(list, mid + 1, last, compare);
		list_inplace_merge(list, first, last, mid, compare);
	}
}

void list_stable_sort(list_t *list, int compare(const void *a, const void *b)) {
	if (list->length > 1) {
		list_inplace_sort(list, 0, list->length - 1, compare);
	}
}

void list_free_items_and_destroy(list_t *list) {
	if (!list) {
		return;
	}

	for (int i = 0; i < list->length; ++i) {
		free(list->items[i]);
	}
	list_free(list);
}

