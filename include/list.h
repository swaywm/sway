#ifndef _SWAY_LIST_H
#define _SWAY_LIST_H

typedef struct {
	int capacity;
	int length;
	void **items;
} list_t;

list_t *create_list(void);
void list_free(list_t *list);
void list_add(list_t *list, void *item);
void list_insert(list_t *list, int index, void *item);
void list_del(list_t *list, int index);
void list_cat(list_t *list, list_t *source);


// ITEM must be a pointer, LIST is list_t*
// ITEM will be NULL after empty list
#define list_for(ITEM, LIST) \
	ITEM = LIST->length ? LIST->items[0] : NULL; \
for(int list_it = 0, list_len = LIST->length; \
	list_it < list_len; \
	ITEM = LIST->items[++list_it])

#endif
