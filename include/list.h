#ifndef _SWAY_LIST_H
#define _SWAY_LIST_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
	size_t capacity;
	size_t length;
	size_t memb_size;
	void *data;
} list_t;

/*
 * Creates a new list with an inital capacity.
 * If capacity is zero, some default value is used instead.
 *
 * memb_size must be the size of the type stored in this list.
 * If an element is added to this list which is not the same type
 * as the elements of the list, the behavior is undefined.
 */
list_t *list_new(size_t memb_size, size_t capacity);

/*
 * Frees a list.
 */
void list_free(list_t *list);

/*
 * Adds an element to the end of the list.
 */
void list_add(list_t *list, const void *data);

/*
 * Adds an element at an arbitrary position in the list, moving
 * the elements past index to make space.
 * index must be less than or equal to the length of the list.
 */
void list_insert(list_t *list, size_t index, const void *data);

/*
 * Deletes the element at an arbitrary position in the list,
 * moving the elements past index into the space.
 * index must be less than the length of the list.
 */
void list_delete(list_t *list, size_t index);

/*
 * Swaps the elements at i1 and i2.
 * i1 and i2 must both be less than the length of the list.
 */
void list_swap(list_t *list, size_t i1, size_t i2);

/*
 * Gets the element of the list at the index.
 * index must be less than the length of the list.
 */
void *list_get(list_t *list, size_t index);

/*
 * Sorts the list using the stdlib qsort() function.
 */
void list_qsort(list_t *list, int compare(const void *, const void *));

/*
 * Sorts the list using insertion sort.
 * This should be used if you need a stable sort, and your list is
 * short and/or nearly sorted.
 */
void list_isort(list_t *list, int compare(const void *, const void *));

/*
 * Returns the index of key in the list, using the stdlib bsearch() function,
 * or -1 if it was not found. If ret is not null, the found element will be
 * copied into it.
 * The list must be sorted.
 */
ssize_t list_bsearch(const list_t *list, int compare(const void *, const void *),
	const void *key, void *ret);

/*
 * Returns the index of the key in the list, using a linear search,
 * or -1 if it was not found. If ret is not null, the found element will be
 * copied into it.
 */
ssize_t list_lsearch(const list_t *list, int compare(const void *, const void *),
	const void *key, void *ret);

/*
 * Calls a function on every item in the list.
 */
void list_foreach(list_t *list, void callback(void *));

/*
 * Returns a pointer to just past the end of the list.
 * This can be used to write for loops more easily.
 *
 * Example (for a list of char *):
 * 	char **end = list_end(list);
 * 	for (char **ptr = list->items; ptr < end; ++ptr) {
 * 		printf("%s\n", *ptr);
 * 	}
 */
// Consider making this inline or __attribute__((pure))__
void *list_end(list_t *list);

#endif
