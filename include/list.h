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
// Insert an item into the list which is already sorted strictly ascending
// according to 'compare'. 'replace' is called when displacing an old item
// and returns the item which will take its place.
void list_sortedset_insert(list_t *list, void* item,
		int compare(const void *item_left, const void *item_right),
		void *replace(void *old_item, void* new_item));
void list_del(list_t *list, int index);
void list_cat(list_t *list, list_t *source);
// See qsort. Remember to use *_qsort functions as compare functions,
// because they dereference the left and right arguments first!
void list_qsort(list_t *list, int compare(const void *left, const void *right));
// Return index for first item in list that returns 0 for given compare
// function or -1 if none matches.
int list_seq_find(list_t *list, int compare(const void *item, const void *cmp_to), const void *cmp_to);
// Requires a list sorted strictly ascending according to 'compare';
// returns the index of the matching item, or -1 is no such item exists
int list_sortedset_find(list_t *list, int compare(const void *item, const void *cmp_to), const void *cmp_to);
// Check if a list is sorted strictly ascending according to compare
int list_is_sortedset(list_t *list, int compare(const void *left, const void *right));
// stable sort since qsort is not guaranteed to be stable
void list_stable_sort(list_t *list, int compare(const void *a, const void *b));
// swap two elements in a list
void list_swap(list_t *list, int src, int dest);
// move item to end of list
void list_move_to_end(list_t *list, void *item);
#endif
