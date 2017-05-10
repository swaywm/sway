#include "list.h"
#include "log.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

list_t *list_new(size_t memb_size, size_t capacity) {
	if (capacity == 0) {
		capacity = 8;
	}

	list_t *list = malloc(sizeof(*list));
	if (!list) {
		return NULL;
	}

	list->capacity = capacity;
	list->length = 0;
	list->memb_size = memb_size;

	list->items = malloc(memb_size * capacity);
	if (!list->items) {
		free(list);
		return NULL;
	}

	return list;
}

void list_free(list_t *list) {
	if (!list) {
		return;
	}

	free(list->items);
	free(list);
}

static bool resize(list_t *list) {
	if (list->length < list->capacity) {
		return true;
	}

	size_t new_cap = list->capacity * 2;
	void *items = realloc(list->items, list->memb_size * new_cap);
	if (!items) {
		return false;
	}

	list->items = items;
	list->capacity = new_cap;
	return true;
}

void list_add(list_t *list, const void *data) {
	if (!sway_assert(list && data, "Invalid argument") || !resize(list)) {
		return;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	memcpy(&array[list->length], data, size);
	++list->length;
}

void list_insert(list_t *list, size_t index, const void *data) {
	if (!sway_assert(list && data && index <= list->length, "Invalid argument") ||
		!resize(list)) {

		return;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;
	memmove(&array[index + 1], &array[index], size * (list->length - index));
	memcpy(&array[index], data, size);
	++list->length;
}

void list_delete(list_t *list, size_t index) {
	if (!sway_assert(list && index < list->length, "Invalid argument")) {
		return;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	memmove(&array[index], &array[index + 1], size * (list->length - index));
	--list->length;

	/* We shrink very sparse lists, but only down to a certain size.
	 * The choice of >= 8 is somewhat arbitrary, but leaves a minimum
	 * size of 4 elements.
	 */
	if (list->length <= list->capacity / 4 && list->capacity >= 8) {
		size_t new_cap = list->capacity / 2;
		void *items = realloc(list->items, list->memb_size * new_cap);
		if (!items) {
			return;
		}

		list->items = items;
		list->capacity = new_cap;
	}
}

void list_swap(list_t *list, size_t i1, size_t i2) {
	if (!sway_assert(list && i1 < list->length && i2 < list->length, "Invalid argument")) {
		return;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	uint8_t tmp[size];
	memcpy(tmp, &array[i1], size);
	memcpy(&array[i1], &array[i2], size);
	memcpy(&array[i2], tmp, size);
}

void *list_get(list_t *list, size_t index) {
	if (!sway_assert(list && index < list->length, "Invalid argument")) {
		return NULL;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	return &array[index];
}

void list_qsort(list_t *list, int compare(const void *, const void *)) {
	if (!sway_assert(list && compare, "Invalid argument")) {
		return;
	}

	qsort(list->items, list->length, list->memb_size, compare);
}

void list_isort(list_t *list, int compare(const void *, const void *)) {
	if (!sway_assert(list && compare, "Invalid argument")) {
		return;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	for (size_t i = 1; i < list->length; ++i) {
		uint8_t tmp[size];
		memcpy(tmp, &array[i], size);

		ssize_t j = i - 1;
		while (j >= 0 && compare(&array[j], tmp) > 0) {
			memcpy(&array[j + 1], &array[j], size);
			--j;
		}

		memcpy(array[j + 1], tmp, size);
	}
}

ssize_t list_bsearch(const list_t *list, int compare(const void *key, const void *item),
		const void *key, void *ret) {

	if (!sway_assert(list && compare && key, "Invalid argument")) {
		return -1;
	}

	const uint8_t *ptr = bsearch(key, list->items, list->length, list->memb_size, compare);
	if (!ptr) {
		return -1;
	} else {
		if (ret) {
			memcpy(ret, ptr, list->memb_size);
		}
		return (ptr - (uint8_t *)list->items) / list->memb_size;
	}
}

ssize_t list_lsearch(const list_t *list, int compare(const void *key, const void *item),
		const void *key, void *ret) {

	if (!sway_assert(list && compare && key, "Invalid argument")) {
		return -1;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	for (size_t i = 0; i < list->length; ++i) {
		if (compare(key, &array[i]) == 0) {
			if (ret) {
				memcpy(ret, &array[i], size);
			}
			return i;
		}
	}

	return -1;
}

void list_foreach(list_t *list, void callback(void *item)) {
	if (!sway_assert(list && callback, "Invalid argument")) {
		return;
	}

	size_t size = list->memb_size;
	uint8_t (*array)[size] = list->items;

	for (size_t i = 0; i < list->length; ++i) {
		callback(&array[i]);
	}
}

void *list_end(list_t *list) {
	if (!sway_assert(list, "Invalid argument")) {
		return NULL;
	}

	return (uint8_t *)list->items + list->memb_size * list->length;
}
