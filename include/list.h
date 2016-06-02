#ifndef _SWAY_LIST_H
#define _SWAY_LIST_H

typedef struct {
	int capacity;
	int length;
	void **items;
} list_t;

list_t *create_list(void);
void list_free(list_t *list);
void list_foreach(list_t *list, void (*callback)(void* item));
void list_add(list_t *list, void *item);
void list_insert(list_t *list, int index, void *item);
void list_del(list_t *list, int index);
void list_cat(list_t *list, list_t *source);
// See qsort. Remember to use *_qsort functions as compare functions,
// because they dereference the left and right arguments first!
void list_qsort(list_t *list, int compare(const void *left, const void *right));
// Return index for first item in list that returns 0 for given compare
// function or -1 if none matches.
int list_seq_find(list_t *list, int compare(const void *item, const void *cmp_to), const void *cmp_to);
// stable sort since qsort is not guaranteed to be stable
void list_stable_sort(list_t *list, int compare(const void *a, const void *b));
#endif
