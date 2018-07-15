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
		list->capacity += 10;
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

void list_foreach(list_t *list, void (*callback)(void *item)) {
	if (list == NULL || callback == NULL) {
		return;
	}
	for (int i = 0; i < list->length; i++) {
		callback(list->items[i]);
	}
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
	int i;
	for (i = 0; i < source->length; ++i) {
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

void list_sortedset_insert(list_t *list, void *item,
		int compare(const void *item_left, const void *item_right),
		void *replace(void *old_item, void* new_item)) {
	if (list->length <= 0) {
		list_add(list, item);
		return;
	}

	size_t lower = 0;
	size_t upper = (size_t)list->length - 1;
	while (lower <= upper && upper != (size_t)-1) {
		size_t div = (lower + upper) / 2;
		int cv = compare(list->items[div], item);
		if (cv < 0) {
			lower = div + 1;
		} else if (cv > 0) {
			upper = div - 1;
		} else {
			list->items[div] = replace(list->items[div], item);
			return;
		}
	}
	list_insert(list, lower, item);
}

int list_sortedset_find(list_t *list,
		int compare(const void *item, const void *cmp_to),
		const void *cmp_to) {
	if (list->length <= 0) {
		return -1;
	}

	size_t lower = 0;
	size_t upper = (size_t)list->length - 1;
	while (lower <= upper && upper != (size_t)-1) {
		size_t div = (lower + upper) / 2;
		int cv = compare(list->items[div], cmp_to);
		if (cv < 0) {
			lower = div + 1;
		} else if (cv > 0) {
			upper = div - 1;
		} else {
			return div;
		}
	}
	return -1;
}

int list_is_sortedset(list_t *list,
		int compare(const void *left, const void *right)) {
	for (size_t i = 1; i < (size_t)list->length; i++) {
		if (compare(list->items[i - 1], list->items[i]) >= 0) {
			return 0;
		}
	}
	return 1;
}
