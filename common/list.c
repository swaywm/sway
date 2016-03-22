#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

list_t *create_list(void) {
	list_t *list = malloc(sizeof(list_t));
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

// Added because IDK if it's safe to remove that memmove
void list_arbitrary_insert(list_t *list, int index, void *item) {
	list_resize(list);
  if(index > list->capacity) return;
	if(list->length < index) list->length = index;
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

void list_qsort(list_t* list, int compare(const void *left, const void *right)) {
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
