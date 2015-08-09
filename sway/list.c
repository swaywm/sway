#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

list_t *create_list() {
	list_t *list = malloc(sizeof(list_t));
	list->capacity = 10;
	list->length = 0;
	list->items = malloc(sizeof(void*) * list->capacity);
	return list;
}

void list_free(list_t *list) {
	if (list == NULL) {
		return;
	}
	free(list->items);
	free(list);
}

void list_add(list_t *list, void *item) {
	if (list->length == list->capacity) {
		list->capacity += 10;
		list->items = realloc(list->items, sizeof(void*) * list->capacity);
	}
	list->items[list->length++] = item;
}

void list_insert(list_t *list, int index, void *item) {
	if (list->length == list->capacity) {
		list->capacity += 10;
		list->items = realloc(list->items, sizeof(void*) * list->capacity);
	}
	list->items[list->length++] = item;
}

void list_del(list_t *list, int index) {
	list->length--;
	memmove(&list->items[index], &list->items[index + 1], sizeof(void*) * (list->capacity - index - 1));
}

void list_cat(list_t *list, list_t *source) {
	int i;
	for (i = 0; i < source->length; ++i) {
		list_add(list, source->items[i]);
	}
}
