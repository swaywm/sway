#ifndef _SWAY_LIST_H
#define _SWAY_LIST_H

typedef struct {
    int capacity;
    int length;
    void **items;
} list_t;

list_t *create_list();
void list_free(list_t *list);
void list_add(list_t *list, void *item);
void list_del(list_t *list, int index);
void list_cat(list_t *list, list_t *source);

#endif
