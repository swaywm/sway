#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "list.h"
#include "log.h"
#include "layout.h"
#include "container.h"
#include "workspace.h"

swayc_t root_container;

void init_layout(void) {
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.children = create_list();
	root_container.handle = -1;
}

static int index_child(swayc_t *parent, swayc_t *child) {
	int i;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			break;
		}
	}
	return i;
}

void add_child(swayc_t *parent, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %dx%d) to %p (%d, %dx%d)", child, child->type,
		child->width, child->height, parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
	child->parent = parent;
	if (parent->focused == NULL) {
		parent->focused = child;
	}
}

swayc_t *add_sibling(swayc_t *sibling, swayc_t *child) {
	swayc_t *parent = sibling->parent;
	int i = index_child(parent, sibling);
	if (i == parent->children->length) {
		--i;
	}
	list_insert(parent->children, i+1, child);
	child->parent = parent;
	return child->parent;
}

swayc_t *replace_child(swayc_t *child, swayc_t *new_child) {
	swayc_t *parent = child->parent;
	if (parent == NULL) {
		return NULL;
	}
	int i = index_child(parent, child);
	parent->children->items[i] = new_child;
	new_child->parent = child->parent;

	if (child->parent->focused == child) {
		child->parent->focused = new_child;
	}
	child->parent = NULL;
	return parent;
}

swayc_t *remove_child(swayc_t *parent, swayc_t *child) {
	int i;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			list_del(parent->children, i);
			break;
		}
	}
	if (parent->focused == child) {
		parent->focused = NULL;
	}
	return parent;
}


void arrange_windows(swayc_t *container, int width, int height) {
	int i;
	if (width == -1 || height == -1) {
		sway_log(L_DEBUG, "Arranging layout for %p", container);
		width = container->width;
		height = container->height;
	}

	int x = 0, y = 0;
	switch (container->type) {
	case C_ROOT:
		for (i = 0; i < container->children->length; ++i) {
			swayc_t *child = container->children->items[i];
			sway_log(L_DEBUG, "Arranging output at %d", x);
			child->x = x;
			child->y = y;
			arrange_windows(child, -1, -1);
			// Removed for now because wlc works with relative positions
			// Addition can be reconsidered once wlc positions are changed
			// x += child->width;
		}
		return;
	case C_OUTPUT:
		container->width = width;
		container->height = height;
		// These lines make x/y negative and result in stuff glitching out
		// Their addition can be reconsidered once wlc positions are changed
		// x -= container->x;
		// y -= container->y;
		for (i = 0; i < container->children->length; ++i) {
			swayc_t *child = container->children->items[i];
			sway_log(L_DEBUG, "Arranging workspace #%d", i);
			child->x = x;
			child->y = y;
			child->width = width;
			child->height = height;
			arrange_windows(child, -1, -1);
		}
		return;
	case C_VIEW:
		{
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
			if (wlc_view_get_state(container->handle) & WLC_BIT_FULLSCREEN) {
				swayc_t *parent = container;
				while (parent->type != C_OUTPUT) {
					parent = parent->parent;
				}
				geometry.origin.x = 0;
				geometry.origin.y = 0;
				geometry.size.w = parent->width;
				geometry.size.h = parent->height;
				wlc_view_set_geometry(container->handle, &geometry);
				wlc_view_bring_to_front(container->handle);
			} else {
				wlc_view_set_geometry(container->handle, &geometry);
				container->width = width;
				container->height = height;
			}
			sway_log(L_DEBUG, "Set view to %d x %d @ %d, %d", geometry.size.w, geometry.size.h,
					geometry.origin.x, geometry.origin.y);
		}
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
	layout_log(&root_container, 0);
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
	while (parent->focused) {
		parent = parent->focused;
	}
	return parent;
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
	while (view != &root_container) {
		view->parent->focused = view;
		view = view->parent;
	}
	while (view && view->type != C_VIEW) {
		view = view->focused;
	}
	if (view) {
		wlc_view_set_state(view->handle, WLC_BIT_ACTIVATED, true);
		wlc_view_focus(view->handle);
	}
}

