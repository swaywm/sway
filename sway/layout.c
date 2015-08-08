#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "list.h"
#include "log.h"
#include "layout.h"

swayc_t root_container;

void arrange_windows() {
	// TODO
}

void init_layout() {
	root_container.type = C_ROOT;
	root_container.layout = L_HORIZ; // TODO: Default layout
	root_container.children = create_list();
	root_container.handle = -1;
}

void free_swayc(swayc_t *container) {
	// NOTE: Does not handle moving children into a different container
	list_free(container->children);
	free(container);
}

swayc_t *get_swayc_for_handle(wlc_handle handle, swayc_t *parent) {
	if (parent->children == NULL) {
		return NULL;
	}
	int i;
	for (i = 0; i < parent->children->length; ++i) {
		swayc_t *child = parent->children->items[i];
		if (child->handle == handle) {
			return child;
		} else {
			swayc_t *res;
			if ((res = get_swayc_for_handle(handle, child))) {
				return res;
			}
		}
	}
	return NULL;
}

swayc_t *get_focused_container(swayc_t *parent) {
	if (parent->focused == NULL) {
		return parent;
	}
	return get_focused_container(parent->focused);
}

void add_view(wlc_handle view_handle) {
	swayc_t *parent = get_focused_container(&root_container);
	sway_log(L_DEBUG, "Adding new view %d under container %d", view_handle, (int)parent->type);

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	swayc_t *view = calloc(1, sizeof(swayc_t));
	view->layout = L_NONE;
	view->handle = view_handle;
	view->parent = parent;
	view->type = C_VIEW;
	list_add(parent->children, view);

	wlc_view_focus(view_handle);

	arrange_windows();
}

void destroy_view(swayc_t *view) {
	sway_log(L_DEBUG, "Destroying container %p", view);
	if (!view->parent) {
		return;
	}

	int i;
	for (i = 0; i < view->parent->children->length; ++i) {
		if (view->parent->children->items[i] == view) {
			list_del(view->parent->children, i);
			break;
		}
	}

	free_swayc(view);

	// TODO: Focus some other window

	arrange_windows();
}

void focus_view(swayc_t *view) {
	if (view == &root_container) {
		return;
	}
	view->parent->focused = view;
	focus_view(view->parent);
}

void add_output(wlc_handle output) {
	sway_log(L_DEBUG, "Adding output %d", output);
	const struct wlc_size* size = wlc_output_get_resolution(output);

	swayc_t *container = calloc(1, sizeof(swayc_t));
	container->handle = output;
	container->type = C_OUTPUT;
	container->children = create_list();
	container->parent = &root_container;
	container->layout = L_NONE;
	container->width = size->w;
	container->height = size->h;

	list_add(root_container.children, container);

	swayc_t *workspace = calloc(1, sizeof(swayc_t));
	workspace->handle = -1;
	workspace->type = C_WORKSPACE;
	workspace->parent = container;
	workspace->width = size->w; // TODO: gaps
	workspace->height = size->h;
	workspace->layout = L_HORIZ; // TODO: Get default layout from config
	workspace->children = create_list();

	list_add(container->children, workspace);

	if (root_container.focused == NULL) {
		focus_view(workspace);
	}
}

void destroy_output(wlc_handle output) {
	sway_log(L_DEBUG, "Destroying output %d", output);
	int i;
	for (i = 0; i < root_container.children->length; ++i) {
		swayc_t *c = root_container.children->items[i];
		if (c->handle == output) {
			list_del(root_container.children, i);
			free_swayc(c);
			return;
		}
	}
}
