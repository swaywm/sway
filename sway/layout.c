#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "log.h"
#include "list.h"
#include "config.h"
#include "container.h"
#include "workspace.h"
#include "focus.h"

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

swayc_t *remove_child(swayc_t *child) {
	int i;
	swayc_t *parent = child->parent;
	if (child->is_floating) {
		// Special case for floating views
		for (i = 0; i < parent->floating->length; ++i) {
			if (parent->floating->items[i] == child) {
				list_del(parent->floating, i);
				break;
			}
		}
	} else {
		for (i = 0; i < parent->children->length; ++i) {
			if (parent->children->items[i] == child) {
				list_del(parent->children, i);
				break;
			}
		}
	}
	//Set focused to new container
	if (parent->focused == child) {
		if (parent->children->length > 0) {
			set_focused_container_for(parent, parent->children->items[i?i-1:0]);
		} else {
			parent->focused = NULL;
		}
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
			child->x = x + container->gaps;
			child->y = y + container->gaps;
			child->width = width - container->gaps * 2;
			child->height = height - container->gaps * 2;
			sway_log(L_DEBUG, "Arranging workspace #%d at %d, %d", i, child->x, child->y);
			arrange_windows(child, -1, -1);
		}
		return;
	case C_VIEW:
		{
			struct wlc_geometry geometry = {
				.origin = {
					.x = container->x + container->gaps,
					.y = container->y + container->gaps
				},
				.size = {
					.w = width - container->gaps * 2,
					.h = height - container->gaps * 2
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
				wlc_view_set_geometry(container->handle, 0, &geometry);
				wlc_view_bring_to_front(container->handle);
			} else {
				wlc_view_set_geometry(container->handle, 0, &geometry);
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

	// Arrage floating layouts for workspaces last
	if (container->type == C_WORKSPACE) {
		for (i = 0; i < container->floating->length; ++i) {
			swayc_t *view = container->floating->items[i];
			if (view->type == C_VIEW) {
				// Set the geometry
				struct wlc_geometry geometry = {
					.origin = {
						.x = view->x,
						.y = view->y
					},
					.size = {
						.w = view->width,
						.h = view->height
					}
				};
				if (wlc_view_get_state(view->handle) & WLC_BIT_FULLSCREEN) {
					swayc_t *parent = view;
					while (parent->type != C_OUTPUT) {
						parent = parent->parent;
					}
					geometry.origin.x = 0;
					geometry.origin.y = 0;
					geometry.size.w = parent->width;
					geometry.size.h = parent->height;
					wlc_view_set_geometry(view->handle, 0, &geometry);
					wlc_view_bring_to_front(view->handle);
				} else {
					wlc_view_set_geometry(view->handle, 0, &geometry);
					// Bring the views to the front in order of the list, the list
					// will be kept up to date so that more recently focused views
					// have higher indexes
					// This is conditional on there not being a fullscreen view in the workspace
					if (!container->focused
							|| !(wlc_view_get_state(container->focused->handle) & WLC_BIT_FULLSCREEN)) {
						wlc_view_bring_to_front(view->handle);
					}
				}
			}
		}
	}
	layout_log(&root_container, 0);
}

swayc_t *get_swayc_for_handle(wlc_handle handle, swayc_t *parent) {
	if (parent->children == NULL) {
		return NULL;
	}

	// Search for floating workspaces
	int i;
	if (parent->type == C_WORKSPACE) {
		for (i = 0; i < parent->floating->length; ++i) {
			swayc_t *child = parent->floating->items[i];
			if (child->handle == handle) {
				return child;
			}
		}
	}

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

