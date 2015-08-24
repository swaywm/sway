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
int min_sane_h = 60;
int min_sane_w = 100;

void init_layout(void) {
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.children = create_list();
	root_container.handle = -1;
}

static int index_child(swayc_t *child) {
	swayc_t *parent = child->parent;
	int i;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			break;
		}
	}
	return i;
}

void add_child(swayc_t *parent, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %fx%f) to %p (%d, %fx%f)", child, child->type,
		child->width, child->height, parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
	child->parent = parent;
	// set focus for this container
	if (parent->children->length == 1) {
		parent->focused = child;
	}
}

void add_floating(swayc_t *ws, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %fx%f) to %p (%d, %fx%f)", child, child->type,
		child->width, child->height, ws, ws->type, ws->width, ws->height);
	list_add(ws->floating, child);
	child->parent = ws;
	child->is_floating = true;
	if (!ws->focused) {
		ws->focused = child;
	}
}

swayc_t *add_sibling(swayc_t *sibling, swayc_t *child) {
	swayc_t *parent = sibling->parent;
	int i = index_child(sibling);
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
	int i = index_child(child);
	parent->children->items[i] = new_child;
	new_child->parent = child->parent;

	// Set parent for new child
	if (child->parent->focused == child) {
		child->parent->focused = new_child;
	}
	child->parent = NULL;
	// Set geometry for new child
	new_child->x = child->x;
	new_child->y = child->y;
	new_child->width = child->width;
	new_child->height = child->height;
	// set child geometry to 0
	child->x = 0;
	child->y = 0;
	child->width = 0;
	child->height = 0;
	return parent;
}

void swap_container(swayc_t *a, swayc_t *b) {
	//TODO doesnt handle floating <-> tiling swap
	if (!sway_assert(a&&b, "%s: parameters must be non null",__func__) ||
		!sway_assert(a->parent && b->parent, "%s: containers must have parents",__func__)) {
		return;
	}
	size_t a_index = index_child(a);
	size_t b_index = index_child(b);
	swayc_t *a_parent = a->parent;
	swayc_t *b_parent = b->parent;
	// Swap the pointers
	a_parent->children->items[a_index] = b;
	b_parent->children->items[b_index] = a;
	a->parent = b_parent;
	b->parent = a_parent;
	if (a_parent->focused == a) {
		a_parent->focused = b;
	}
	// dont want to double switch
	if (b_parent->focused == b && a_parent != b_parent) {
		b_parent->focused = a;
	}
	// and their geometry
	double x = a->x;
	double y = a->y;
	double w = a->width;
	double h = a->height;
	a->x = b->x;
	a->y = b->y;
	a->width = b->width;
	a->height = b->height;
	b->x = x;
	b->y = y;
	b->width = w;
	b->height = h;
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
		i = 0;
	} else {
		for (i = 0; i < parent->children->length; ++i) {
			if (parent->children->items[i] == child) {
				list_del(parent->children, i);
				break;
			}
		}
	}
	// Set focused to new container
	if (parent->focused == child) {
		if (parent->children->length > 0) {
			parent->focused = parent->children->items[i?i-1:0];
		} else {
			parent->focused = NULL;
		}
	}
	return parent;
}

//TODO: Implement horizontal movement.
//TODO: Implement move to a different workspace.
void move_container(swayc_t *container,swayc_t* root,enum movement_direction direction){
	sway_log(L_DEBUG, "Moved window");
	swayc_t *temp;
	int i;
	int clength = root->children->length;
	//Rearrange
	for (i = 0; i < clength; ++i) {
		swayc_t *child = root->children->items[i];
		if (child->handle == container->handle){
			if (clength == 1){
				//Only one container, meh.
				break;
			}
			if (direction == MOVE_LEFT && i > 0){
				temp = root->children->items[i-1];
				root->children->items[i] = temp;
				root->children->items[i-1] = container;
				arrange_windows(&root_container,-1,-1);
			}
			else if (direction == MOVE_RIGHT && i < clength-1){
				temp = root->children->items[i+1];
				root->children->items[i] = temp;
				root->children->items[i+1] = container;
				arrange_windows(&root_container,-1,-1);

			}
			else if (direction == MOVE_UP){
				sway_log(L_INFO, "Moving up not implemented");
			}
			else if (direction == MOVE_DOWN){
				sway_log(L_INFO, "Moving down not implemented");
			}

			break;
		}
		else if (child->children != NULL){
			move_container(container,child,direction);
		}
	}

}

void update_geometry(swayc_t *container) {
	if (container->type != C_VIEW) {
		return;
	}
	struct wlc_geometry geometry = {
		.origin = {
			.x = container->x + (container->is_floating ? 0 : container->gaps / 2),
			.y = container->y + (container->is_floating ? 0 : container->gaps / 2)
		},
		.size = {
			.w = container->width - (container->is_floating ? 0 : container->gaps),
			.h = container->height - (container->is_floating ? 0 : container->gaps)
		}
	};
	if (swayc_is_fullscreen(container)) {
		swayc_t *parent = swayc_parent_by_type(container, C_OUTPUT);
		geometry.origin.x = 0;
		geometry.origin.y = 0;
		geometry.size.w = parent->width;
		geometry.size.h = parent->height;
	}
	wlc_view_set_geometry(container->handle, 0, &geometry);
	return;
}

void arrange_windows(swayc_t *container, double width, double height) {
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
			arrange_windows(child, -1, -1);
			x += child->width;
		}
		return;
	case C_OUTPUT:
		container->width = width;
		container->height = height;
		x = 0, y = 0;
		for (i = 0; i < container->children->length; ++i) {
			swayc_t *child = container->children->items[i];
			child->x = x + container->gaps;
			child->y = y + container->gaps;
			child->width = width - container->gaps * 2;
			child->height = height - container->gaps * 2;
			sway_log(L_DEBUG, "Arranging workspace #%d at %f, %f", i, child->x, child->y);
			arrange_windows(child, -1, -1);
		}
		return;
	case C_VIEW:
		{
			container->width = width;
			container->height = height;
			update_geometry(container);
			sway_log(L_DEBUG, "Set view to %.f x %.f @ %.f, %.f", container->width,
					container->height, container->x, container->y);
		}
		return;
	default:
		container->width = width;
		container->height = height;
		break;
	}

	x = y = 0;
	double scale = 0;
	switch (container->layout) {
	case L_HORIZ:
	default:
		// Calculate total width
		for (i = 0; i < container->children->length; ++i) {
			double *old_width = &((swayc_t *)container->children->items[i])->width;
			if (*old_width <= 0) {
				if (container->children->length > 1) {
					*old_width = width / (container->children->length - 1);
				} else {
					*old_width = width;
				}
			}
			scale += *old_width;
		}
		// Resize windows
		if (scale > 0.1) {
			scale = width / scale;
			sway_log(L_DEBUG, "Arranging %p horizontally", container);
			for (i = 0; i < container->children->length; ++i) {
				swayc_t *child = container->children->items[i];
				sway_log(L_DEBUG, "Calculating arrangement for %p:%d (will scale %f by %f)", child, child->type, width, scale);
				child->x = x + container->x;
				child->y = y + container->y;
				arrange_windows(child, child->width * scale, height);
				x += child->width;
			}
		}
		break;
	case L_VERT:
		// Calculate total height
		for (i = 0; i < container->children->length; ++i) {
			double *old_height = &((swayc_t *)container->children->items[i])->height;
			if (*old_height <= 0) {
				if (container->children->length > 1) {
					*old_height = height / (container->children->length - 1);
				} else {
					*old_height = height;
				}
			}
			scale += *old_height;
		}
		// Resize
		if (scale > 0.1) {
			scale = height / scale;
			sway_log(L_DEBUG, "Arranging %p vertically", container);
			for (i = 0; i < container->children->length; ++i) {
				swayc_t *child = container->children->items[i];
				sway_log(L_DEBUG, "Calculating arrangement for %p:%d (will scale %f by %f)", child, child->type, height, scale);
				child->x = x + container->x;
				child->y = y + container->y;
				arrange_windows(child, width, child->height * scale);
				y += child->height;
			}
		}
		break;
	}

	// Arrage floating layouts for workspaces last
	if (container->type == C_WORKSPACE) {
		for (i = 0; i < container->floating->length; ++i) {
			swayc_t *view = container->floating->items[i];
			if (view->type == C_VIEW) {
				update_geometry(view);
				if (swayc_is_fullscreen(view)) {
					wlc_view_bring_to_front(view->handle);
				} else if (!container->focused
						|| !swayc_is_fullscreen(container->focused)) {
					wlc_view_bring_to_front(view->handle);
				}
			}
		}
	}
	layout_log(&root_container, 0);
}

swayc_t *get_swayc_in_direction_under(swayc_t *container, enum movement_direction dir, swayc_t *limit) {
	swayc_t *parent = container->parent;

	if (dir == MOVE_PARENT) {
		if (parent->type == C_OUTPUT) {
			return NULL;
		} else {
			return parent;
		}
	}
	while (true) {
		// Test if we can even make a difference here
		bool can_move = false;
		int diff = 0;
		int i;
		if (parent->type == C_ROOT) {
			// Find the next output
			int target = -1, max_x = 0, max_y = 0, self = -1;
			sway_log(L_DEBUG, "Moving between outputs");
			
			for (i = 0; i < parent->children->length; ++i) {
				swayc_t *next = parent->children->items[i];
				if (next == container) {
					self = i;
					sway_log(L_DEBUG, "self is %p %d", next, self);
					continue;
				}
				if (next->type == C_OUTPUT) {
					sway_log(L_DEBUG, "Testing with %p %d (dir %d)", next, i, dir);
					// Check if it's more extreme
					if (dir == MOVE_RIGHT) {
						if (container->x + container->width <= next->x) {
							if (target == -1 || next->x < max_x) {
								target = i;
								max_x = next->x;
							}
						}
					} else if (dir == MOVE_LEFT) {
						if (container->x >= next->x + next->width) {
							if (target == -1 || max_x < next->x) {
								target = i;
								max_x = next->x;
							}
						}
					} else if (dir == MOVE_DOWN) {
						if (container->y + container->height <= next->y) {
							if (target == -1 || next->y < max_y) {
								target = i;
								max_y = next->y;
							}
						}
					} else if (dir == MOVE_UP) {
						if (container->y >= next->y + next->height) {
							if (target == -1 || max_y < next->y) {
								target = i;
								max_y = next->y;
							}
						}
					}
				}
			}

			if (target == -1) {
				can_move = false;
			} else {
				can_move = true;
				diff = target - self;
			}
		} else {
			if (dir == MOVE_LEFT || dir == MOVE_RIGHT) {
				if (parent->layout == L_HORIZ) {
					can_move = true;
					diff = dir == MOVE_LEFT ? -1 : 1;
				}
			} else {
				if (parent->layout == L_VERT) {
					can_move = true;
					diff = dir == MOVE_UP ? -1 : 1;
				}
			}
		}

		if (can_move) {
			for (i = 0; i < parent->children->length; ++i) {
				swayc_t *child = parent->children->items[i];
				if (child == container) {
					break;
				}
			}
			int desired = i + diff;
			if (desired < 0 || desired >= parent->children->length) {
				can_move = false;
			} else {
				return parent->children->items[desired];
			}
		}
		if (!can_move) {
			container = parent;
			parent = parent->parent;
			if (!parent || container == limit) {
				// Nothing we can do
				return NULL;
			}
		}
	}
}

swayc_t *get_swayc_in_direction(swayc_t *container, enum movement_direction dir) {
	return get_swayc_in_direction_under(container, dir, NULL);
}

void recursive_resize(swayc_t *container, double amount, enum wlc_resize_edge edge) {
	int i;
	bool layout_match = true;
	sway_log(L_DEBUG, "Resizing %p with amount: %f", container, amount);
	if (edge == WLC_RESIZE_EDGE_LEFT || edge == WLC_RESIZE_EDGE_RIGHT) {
		container->width += amount;
		layout_match = container->layout == L_HORIZ;
	} else if (edge == WLC_RESIZE_EDGE_TOP || edge == WLC_RESIZE_EDGE_BOTTOM) {
		container->height += amount;
		layout_match = container->layout == L_VERT;
	}
	if (container->type == C_VIEW) {
		struct wlc_geometry geometry = {
			.origin = {
				.x = container->x + container->gaps / 2,
				.y = container->y + container->gaps / 2
			},
			.size = {
				.w = container->width - container->gaps,
				.h = container->height - container->gaps
			}
		};
		wlc_view_set_geometry(container->handle, edge, &geometry);
		return;
	}
	if (layout_match) {
		for (i = 0; i < container->children->length; i++) {
			recursive_resize(container->children->items[i], amount/container->children->length, edge);
		}
	} else {
		for (i = 0; i < container->children->length; i++) {
			recursive_resize(container->children->items[i], amount, edge);
		}
	}
}
