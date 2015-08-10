#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "list.h"
#include "log.h"
#include "layout.h"

swayc_t root_container;

swayc_t *find_container(swayc_t *container, bool (*test)(swayc_t *view, void *data), void *data) {
	if (!container->children) {
		return NULL;
	}
	int i;
	for (i = 0; i < container->children->length; ++i) {
		swayc_t *child = container->children->items[i];
		if (test(child, data)) {
			return child;
		} else {
			swayc_t *_ = find_container(child, test, data);
			if (_) {
				return _;
			}
		}
	}
	return NULL;
}

void arrange_windows(swayc_t *container, int width, int height) {
	int i;
	if (width == -1 || height == -1) {
		sway_log(L_DEBUG, "Arranging layout for %p", container);
		width = container->width;
		height = container->height;
	}

	switch (container->type) {
	case C_ROOT:
		for (i = 0; i < container->children->length; ++i) {
			arrange_windows(container->children->items[i], -1, -1);
		}
		return;
	case C_VIEW:
		sway_log(L_DEBUG, "Setting view to %d x %d @ %d, %d", width, height, container->x, container->y);
		// If the view is fullscreen, we need to tell wlc to draw it as such
		if (wlc_view_get_state(container->handle) & WLC_BIT_FULLSCREEN > 0) {
			sway_log(L_DEBUG, "window is fullscreen!");
		}
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
		container->width = width;
		container->height = height;
		return;
	default:
		container->width = width;
		container->height = height;
		break;
	}

	double total_weight = 0;
	for (i = 0; i < container->children->length; ++i) {
		swayc_t *child = container->children->items[i];
		total_weight += child->weight;
	}

	int x = 0, y = 0;
	switch (container->layout) {
	case L_HORIZ:
	default:
		sway_log(L_DEBUG, "Arranging %p horizontally", container);
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
	case L_VERT:
		sway_log(L_DEBUG, "Arranging %p vertically", container);
		for (i = 0; i < container->children->length; ++i) {
			swayc_t *child = container->children->items[i];
			double percent = child->weight / total_weight;
			sway_log(L_DEBUG, "Calculating arrangement for %p:%d (will receive %.2f of %d)", child, child->type, percent, width);
			child->x = x + container->x;
			child->y = y + container->y;
			int w = width;
			int h = height * percent;
			arrange_windows(child, w, h);
			y += h;
		}
		break;
	}
}

void init_layout() {
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.children = create_list();
	root_container.handle = -1;
}

void free_swayc(swayc_t *container) {
	// NOTE: Does not handle moving children into a different container
	list_free(container->children);
	if (container->name) {
		free(container->name);
	}
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
	const uint32_t type = wlc_view_get_type(view_handle);
	const char *title = wlc_view_get_title(view_handle);
	if ((type & WLC_BIT_UNMANAGED) || (type & WLC_BIT_POPUP) || (type & WLC_BIT_MODAL) || (type & WLC_BIT_SPLASH)) {
		sway_log(L_DEBUG, "Leaving view %d:%s alone (unmanaged)", view_handle, title);
		return;
	}

	swayc_t *parent = get_focused_container(&root_container);
	sway_log(L_DEBUG, "Adding new view %d:%s under container %p %d", view_handle, title, parent, parent->type);

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	swayc_t *view = calloc(1, sizeof(swayc_t));
	view->weight = 1;
	view->layout = L_NONE;
	view->handle = view_handle;
	view->parent = parent;
	view->type = C_VIEW;
	view->name = malloc(strlen(title) + 1);
	strcpy(view->name, title);
	add_child(parent, view);

	unfocus_all(&root_container);
	focus_view(view);

	arrange_windows(parent, -1, -1);
}

int remove_container_from_parent(swayc_t *parent, swayc_t *container) {
	int i;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == container) {
			list_del(parent->children, i);
			break;
		}
	}

	if (parent->focused == container) {
		parent->focused = NULL;
	}

	return i;
}

void destroy_view(swayc_t *view) {
	if (view == NULL) {
		sway_log(L_DEBUG, "Warning: NULL passed into destroy_view");
		return;
	}
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

	if (parent->focused == view) {
		parent->focused = NULL;
	}

	unfocus_all(&root_container);
	if (parent->children->length != 0) {
		focus_view(parent->children->items[0]);
	} else {
		focus_view(parent);
	}

	arrange_windows(parent, -1, -1);

	if (parent->children->length == 0 && parent->type == C_CONTAINER) {
		destroy_view(parent);
	}
}

void unfocus_all(swayc_t *container) {
	if (container->children == NULL) {
		return;
	}
	int i;
	for (i = 0; i < container->children->length; ++i) {
		swayc_t *view = container->children->items[i];
		if (view->type == C_VIEW) {
			wlc_view_set_state(view->handle, WLC_BIT_ACTIVATED, false);
		} else {
			unfocus_all(view);
		}
	}
}

void focus_view(swayc_t *view) {
	sway_log(L_DEBUG, "Setting focus for %p", view);
	if (view == &root_container) {
		// Propegate wayland focus down
		swayc_t *child = view->focused;
		while (child && child->type != C_VIEW) {
			child = child->focused;
		}
		if (child) {
			wlc_view_set_state(child->handle, WLC_BIT_ACTIVATED, true);
			wlc_view_focus(child->handle);
		}
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

swayc_t *create_container(swayc_t *parent, wlc_handle handle) {
	swayc_t *c = calloc(1, sizeof(swayc_t));
	c->weight = 1;
	c->handle = handle;
	c->parent = parent;
	c->layout = L_NONE;
	c->type = C_CONTAINER;
	c->children = create_list();
	return c;
}

void add_output(wlc_handle output) {
	sway_log(L_DEBUG, "Adding output %d", output);
	const struct wlc_size* size = wlc_output_get_resolution(output);

	swayc_t *container = create_container(&root_container, output);
	container->type = C_OUTPUT;
	container->width = size->w;
	container->height = size->h;
	add_child(&root_container, container);

	swayc_t *workspace = create_container(container, -1);
	workspace->type = C_WORKSPACE;
	workspace->width = size->w; // TODO: gaps
	workspace->height = size->h;
	workspace->layout = L_HORIZ; // TODO: Get default layout from config
	add_child(container, workspace);

	if (root_container.focused == NULL) {
		unfocus_all(&root_container);
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
