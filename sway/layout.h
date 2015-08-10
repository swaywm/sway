#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlc/wlc.h>
#include "list.h"

struct sway_container {
	wlc_handle handle;

	enum {
		C_ROOT,
		C_OUTPUT,
		C_WORKSPACE,
		C_CONTAINER,
		C_VIEW
	} type;

	enum {
		L_NONE,
		L_HORIZ,
		L_VERT,
		L_STACKED,
		L_TABBED,
		L_FLOATING
	} layout;

	// Not including borders or margins
	int width, height;

	int x, y;

	bool visible;

	int weight;

	char *name;

	list_t *children;

	struct sway_container *parent;
	struct sway_container *focused;
};

typedef struct sway_container swayc_t;

extern swayc_t root_container;

void init_layout();
void add_child(swayc_t *parent, swayc_t *child);
void add_output(wlc_handle output);
void destroy_output(wlc_handle output);
void destroy_view(swayc_t *view);
void add_view(wlc_handle view);
void unfocus_all(swayc_t *container);
void focus_view(swayc_t *view);
void arrange_windows(swayc_t *container, int width, int height);
swayc_t *find_container(swayc_t *container, bool (*test)(swayc_t *view, void *data), void *data);
swayc_t *get_focused_container(swayc_t *parent);
int remove_container_from_parent(swayc_t *parent, swayc_t *container);
swayc_t *create_container(swayc_t *parent, wlc_handle handle);
swayc_t *get_swayc_for_handle(wlc_handle handle, swayc_t *parent);

#endif
