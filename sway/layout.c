#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "list.h"
#include "log.h"
#include "layout.h"

swayc_t root_container;

void arrange_windows(swayc_t *container, int width, int height) {
	int i;
	if (width == -1 || height == -1) {
		sway_log(L_DEBUG, "Arranging layout for %p", container);
	}
	if (width == -1) {
		width = container->width;
	}
	if (height == -1) {
		height = container->height;
	}

	if (container->type == C_ROOT) {
		for (i = 0; i < container->children->length; ++i) {
			arrange_windows(container->children->items[i], -1, -1);
		}
		return;
	}

	if (container->type == C_VIEW) {
		sway_log(L_DEBUG, "Setting view to %d x %d @ %d, %d", width, height, container->x, container->y);
		struct wlc_geometry geometry = {
			.origin = {
				.x = container->x,
				.y = container->y
			},
			.size = {
				.w = width,
				.h = height
			}
		};
		wlc_view_set_geometry(container->handle, &geometry);
		return;
	}
	container->width = width;
	container->height = height;

	double total_weight = 0;
	for (i = 0; i < container->children->length; ++i) {
		swayc_t *child = container->children->items[i];
		total_weight += child->weight;
	}

	int x = 0, y = 0;
	switch (container->layout) {
	case L_HORIZ:
	default:
		for (i = 0; i < container->children->length; ++i) {
			swayc_t *child = container->children->items[i];
			double percent = child->weight / total_weight;
			sway_log(L_DEBUG, "Calculating arrangement for %p:%d (will receive %.2f of %d)", child, child->type, percent, width);
			child->x = x + container->x;
			child->y = y + container->y;
			int w = width * percent;
			int h = height;
			arrange_windows(child, w, h);
			x += w;
		}
		break;
	}
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
	sway_log(L_DEBUG, "Adding new view %d under container %p %d", view_handle, parent, parent->type);

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	swayc_t *view = calloc(1, sizeof(swayc_t));
	view->weight = 1;
	view->layout = L_NONE;
	view->handle = view_handle;
	view->parent = parent;
	view->type = C_VIEW;
	add_child(parent, view);

	wlc_view_focus(view_handle);

	arrange_windows(parent, -1, -1);
}

void destroy_view(swayc_t *view) {
	sway_log(L_DEBUG, "Destroying container %p", view);
	swayc_t *parent = view->parent;
	if (!parent) {
		return;
	}

	int i;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == view) {
			list_del(parent->children, i);
			break;
		}
	}

	free_swayc(view);

	// TODO: Focus some other window

	arrange_windows(parent, -1, -1);
}

void focus_view(swayc_t *view) {
	if (view == &root_container) {
		return;
	}
	view->parent->focused = view;
	focus_view(view->parent);
}

void add_child(swayc_t *parent, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %dx%d) to %p (%d, %dx%d)", child, child->type,
			child->width, child->height, parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
}

void add_output(wlc_handle output) {
	sway_log(L_DEBUG, "Adding output %d", output);
	const struct wlc_size* size = wlc_output_get_resolution(output);

	swayc_t *container = calloc(1, sizeof(swayc_t));
	container->weight = 1;
	container->handle = output;
	container->type = C_OUTPUT;
	container->children = create_list();
	container->parent = &root_container;
	container->layout = L_NONE;
	container->width = size->w;
	container->height = size->h;
	add_child(&root_container, container);

	swayc_t *workspace = calloc(1, sizeof(swayc_t));
	workspace->weight = 1;
	workspace->handle = -1;
	workspace->type = C_WORKSPACE;
	workspace->parent = container;
	workspace->width = size->w; // TODO: gaps
	workspace->height = size->h;
	workspace->layout = L_HORIZ; // TODO: Get default layout from config
	workspace->children = create_list();
	add_child(container, workspace);

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
