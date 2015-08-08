#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "list.h"
#include "log.h"
#include "layout.h"

list_t *outputs;
wlc_handle focused_view;

void arrange_windows() {
}

void init_layout() {
	outputs = create_list();
	focused_view = -1;
}

struct sway_container *get_container(wlc_handle output, int *index) {
	int i;
	for (i = 0; i < outputs->length; ++i) {
		struct sway_container *c = outputs->items[i];
		if (c->output == output) {
			return c;
		}
	}
	return NULL;
}

struct sway_container *get_container_for_view_recurse(wlc_handle handle, int *index, struct sway_container *parent) {
	int j;
	for (j = 0; j < parent->children->length; ++j) {
		struct sway_container *child = parent->children->items[j];
		if (child->layout == LAYOUT_IS_VIEW) {
			if (child->output == handle) {
				*index = j;
				return parent;
			}
		} else {
			struct sway_container *res;
			if ((res = get_container_for_view_recurse(handle, index, child))) {
				return res;
			}
		}
	}
	return NULL;
}

struct sway_container *get_container_for_view(wlc_handle handle, int *index) {
	int i;
	for (i = 0; i < outputs->length; ++i) {
		struct sway_container *c = outputs->items[i];
		struct sway_container *res;
		if ((res = get_container_for_view_recurse(handle, index, c))) {
			return res;
		}
	}
	return NULL;
}

void add_view(wlc_handle view_handle) {
	struct sway_container *container;
	int _;

	if (focused_view == -1) { // Add it to the output container
		sway_log(L_DEBUG, "Adding initial view for output", view_handle);
		wlc_handle output = wlc_get_focused_output();
		container = get_container(output, &_);
	} else {
		sway_log(L_DEBUG, "Adding view %d to output", view_handle);
		// TODO
	}

	// Create "container" for this view
	struct sway_container *view = malloc(sizeof(struct sway_container));
	view->layout = LAYOUT_IS_VIEW;
	view->children = NULL;
	view->output = view_handle;
	list_add(container->children, view);

	wlc_view_focus(view_handle);

	arrange_windows();
}

void destroy_view(wlc_handle view) {
	sway_log(L_DEBUG, "Destroying view %d", view);

	int index;
	struct sway_container *container = get_container_for_view(view, &index);
	list_del(container->children, index);

	wlc_view_focus(get_topmost(wlc_view_get_output(view), 0));

	arrange_windows();
}

void add_output(wlc_handle output) {
	struct sway_container *container = malloc(sizeof(struct sway_container));
	// TODO: Get default layout from config
	container->output = output;
	container->children = create_list();
	container->layout = LAYOUT_TILE_HORIZ;
	list_add(outputs, container);
}

void destroy_output(wlc_handle output) {
	int index;
	struct sway_container *c = get_container(output, &index);
	// TODO: Move all windows in this output somewhere else?
	// I don't think this will ever be called unless we destroy the output ourselves
	if (!c) {
		return;
	}
	list_del(outputs, index);
}

wlc_handle get_topmost(wlc_handle output, size_t offset) {
   size_t memb;
   const wlc_handle *views = wlc_output_get_views(output, &memb);
   return (memb > 0 ? views[(memb - 1 + offset) % memb] : 0);
}
