#include "list.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

list_t *list_new(size_t memb_size, size_t capacity) {
	if (capacity == 0)
		capacity = 8;

	list_t *l = malloc(sizeof(*l) + memb_size * capacity);
	if (!l) {
		return NULL;
	}

	l->capacity = capacity;
	l->length = 0;
	l->memb_size = memb_size;

	return l;
}

static bool resize(list_t **list) {
	list_t *l = *list;

	if (l->length < l->capacity) {
		return true;
	}

	size_t new_cap = l->capacity * 2;
	l = realloc(l, sizeof(*l) + l->memb_size * new_cap);
	if (!l) {
		return false;
	}

	*list = l;
	l->capacity = new_cap;
	return true;
}

void list_add(list_t **list, const void *data) {
	if (!data || !list || !*list || !resize(list)) {
		return;
	}
	list_t *l = *list;

	memcpy(l->data + l->memb_size * l->length, data, l->memb_size);
	++l->length;
}

void list_insert(list_t **list, size_t index, const void *data) {
	if (!data || !list || !*list || index > (*list)->length || !resize(list)) {
		return;
	}
	list_t *l = *list;

	size_t size = l->memb_size;
	unsigned char (*array)[size] = (void *)l->data;
	memmove(&array[index + 1], &array[index], size * (l->length - index));
	memcpy(&array[index], data, size);
	++l->length;
}

void list_delete(list_t *list, size_t index) {
	if (!list || index >= list->length) {
		return;
	}

	size_t size = list->memb_size;
	unsigned char (*array)[size] = (void *)list->data;

	memmove(&array[index], &array[index + 1], size * (list->length - index));
	--list->length;
}

void list_swap(list_t *list, size_t i1, size_t i2) {
	if (!list || i1 >= list->length || i2 >= list->length) {
		return;
	}

	size_t size = list->memb_size;
	unsigned char (*array)[size] = (void *)list->data;

	unsigned char tmp[size];
	memcpy(tmp, &array[i1], size);
	memcpy(&array[i1], &array[i2], size);
	memcpy(&array[i2], tmp, size);
}

void *list_get(list_t *list, size_t index) {
	if (!list || index >= list->length) {
		return NULL;
	}

	return list->data + list->memb_size * index;
}

void list_qsort(list_t *list, int compare(const void *, const void *)) {
	if (!list || !compare) {
		return;
	}

	qsort(list->data, list->length, list->memb_size, compare);
}

void list_isort(list_t *list, int compare(const void *, const void *)) {
	if (!list || !compare) {
		return;
	}

	size_t size = list->memb_size;
	unsigned char (*array)[size] = (void *)list->data;

	for (size_t i = 1; i < list->length; ++i) {
		unsigned char tmp[size];
		memcpy(tmp, &array[i], size);

		ssize_t j = i - 1;
		while (j >= 0 && compare(&array[j], tmp) > 0) {
			memcpy(&array[j + 1], &array[j], size);
			--j;
		}

		memcpy(array[j + 1], tmp, size);
	}
}

ssize_t list_bsearch(const list_t *list, int compare(const void *, const void *),
	const void *key, void *ret) {
	if (!list || !compare || !key) {
		return -1;
	}

	const unsigned char *ptr = bsearch(key, list->data, list->length, list->memb_size, compare);
	if (!ptr) {
		return -1;
	} else {
		if (ret) {
			memcpy(ret, ptr, list->memb_size);
		}
		return (ptr - list->data) / list->memb_size;
	}
}

ssize_t list_lsearch(const list_t *list, int compare(const void *, const void *),
	const void *key, void *ret) {
	if (!list || !compare || !key) {
		return -1;
	}

	size_t size = list->memb_size;
	unsigned char (*array)[size] = (void *)list->data;

	for (size_t i = 0; i < list->length; ++i) {
		if (compare(&array[i], key) == 0) {
			if (ret) {
				memcpy(ret, &array[i], size);
			}
			return i;
		}
	}

	return -1;
}

void list_foreach_cb(list_t *list, void callback(void *)) {
	if (!list || !callback) {
		return;
	}

	size_t size = list->memb_size;
	unsigned char (*array)[size] = (void *)list->data;

	for (size_t i = 0; i < list->length; ++i) {
		callback(&array[i]);
	}
}
