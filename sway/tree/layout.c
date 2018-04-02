#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "sway/tree/view.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "list.h"
#include "log.h"

struct sway_container root_container;

static void output_layout_change_notify(struct wl_listener *listener,
		void *data) {
	struct wlr_box *layout_box = wlr_output_layout_get_box(
		root_container.sway_root->output_layout, NULL);
	root_container.width = layout_box->width;
	root_container.height = layout_box->height;

	for (int i = 0 ; i < root_container.children->length; ++i) {
		struct sway_container *output_container =
			root_container.children->items[i];
		if (output_container->type != C_OUTPUT) {
			continue;
		}
		struct sway_output *output = output_container->sway_output;

		struct wlr_box *output_box = wlr_output_layout_get_box(
			root_container.sway_root->output_layout, output->wlr_output);
		if (!output_box) {
			continue;
		}
		output_container->x = output_box->x;
		output_container->y = output_box->y;
		output_container->width = output_box->width;
		output_container->height = output_box->height;
	}

	arrange_windows(&root_container, -1, -1);
}

void layout_init(void) {
	root_container.id = 0; // normally assigned in new_swayc()
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.name = strdup("root");
	root_container.children = create_list();
	wl_signal_init(&root_container.events.destroy);

	root_container.sway_root = calloc(1, sizeof(*root_container.sway_root));
	root_container.sway_root->output_layout = wlr_output_layout_create();
	wl_list_init(&root_container.sway_root->unmanaged_views);
	wl_signal_init(&root_container.sway_root->events.new_container);

	root_container.sway_root->output_layout_change.notify =
		output_layout_change_notify;
	wl_signal_add(&root_container.sway_root->output_layout->events.change,
		&root_container.sway_root->output_layout_change);
}

static int index_child(const struct sway_container *child) {
	// TODO handle floating
	struct sway_container *parent = child->parent;
	int i, len;
	len = parent->children->length;
	for (i = 0; i < len; ++i) {
		if (parent->children->items[i] == child) {
			break;
		}
	}

	if (!sway_assert(i < len, "Stray container")) {
		return -1;
	}
	return i;
}

struct sway_container *container_add_sibling(struct sway_container *fixed,
		struct sway_container *active) {
	// TODO handle floating
	struct sway_container *parent = fixed->parent;
	int i = index_child(fixed);
	list_insert(parent->children, i + 1, active);
	active->parent = parent;
	return active->parent;
}

void container_add_child(struct sway_container *parent,
		struct sway_container *child) {
	wlr_log(L_DEBUG, "Adding %p (%d, %fx%f) to %p (%d, %fx%f)",
			child, child->type, child->width, child->height,
			parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
	child->parent = parent;
}

struct sway_container *container_reap_empty(struct sway_container *container) {
	if (!sway_assert(container, "reaping null container")) {
		return NULL;
	}
	wlr_log(L_DEBUG, "reaping %p %s", container, container->name);
	while (container != &root_container && container->children->length == 0) {
		if (container->type == C_WORKSPACE) {
			if (!workspace_is_visible(container)) {
				struct sway_container *parent = container->parent;
				container_workspace_destroy(container);
				return parent;
			}
			return container;
		} else if (container->type == C_CONTAINER) {
			struct sway_container *parent = container->parent;
			container_destroy(container);
			container = parent;
		} else {
			container = container->parent;
		}
	}
	return container;
}

struct sway_container *container_remove_child(struct sway_container *child) {
	struct sway_container *parent = child->parent;
	for (int i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			list_del(parent->children, i);
			break;
		}
	}
	child->parent = NULL;
	return container_reap_empty(parent);
}

void container_move_to(struct sway_container* container,
		struct sway_container* destination) {
	if (container == destination
			|| container_has_anscestor(container, destination)) {
		return;
	}
	struct sway_container *old_parent = container_remove_child(container);
	container->width = container->height = 0;
	struct sway_container *new_parent =
		container_add_sibling(destination, container);
	if (old_parent) {
		arrange_windows(old_parent, -1, -1);
	}
	arrange_windows(new_parent, -1, -1);
}

enum sway_container_layout container_get_default_layout(
		struct sway_container *output) {
	if (config->default_layout != L_NONE) {
		return config->default_layout;
	} else if (config->default_orientation != L_NONE) {
		return config->default_orientation;
	} else if (output->width >= output->height) {
		return L_HORIZ;
	} else {
		return L_VERT;
	}
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	struct sway_container *a = *(void **)_a;
	struct sway_container *b = *(void **)_b;
	int retval = 0;

	if (isdigit(a->name[0]) && isdigit(b->name[0])) {
		int a_num = strtol(a->name, NULL, 10);
		int b_num = strtol(b->name, NULL, 10);
		retval = (a_num < b_num) ? -1 : (a_num > b_num);
	} else if (isdigit(a->name[0])) {
		retval = -1;
	} else if (isdigit(b->name[0])) {
		retval = 1;
	}

	return retval;
}

void container_sort_workspaces(struct sway_container *output) {
	list_stable_sort(output->children, sort_workspace_cmp_qsort);
}

static void apply_horiz_layout(struct sway_container *container, const double x,
				const double y, const double width,
				const double height, const int start,
				const int end);

static void apply_vert_layout(struct sway_container *container, const double x,
				const double y, const double width,
				const double height, const int start,
				const int end);

void arrange_windows(struct sway_container *container,
		double width, double height) {
	int i;
	if (width == -1 || height == -1) {
		width = container->width;
		height = container->height;
	}
	// pixels are indivisible. if we don't round the pixels, then the view
	// calculations will be off (e.g. 50.5 + 50.5 = 101, but in reality it's
	// 50 + 50 = 100). doing it here cascades properly to all width/height/x/y.
	width = floor(width);
	height = floor(height);

	wlr_log(L_DEBUG, "Arranging layout for %p %s %fx%f+%f,%f", container,
		container->name, container->width, container->height, container->x,
		container->y);

	double x = 0, y = 0;
	switch (container->type) {
	case C_ROOT:
		// TODO: wlr_output_layout probably
		for (i = 0; i < container->children->length; ++i) {
			struct sway_container *output = container->children->items[i];
			wlr_log(L_DEBUG, "Arranging output '%s' at %f,%f",
					output->name, output->x, output->y);
			arrange_windows(output, -1, -1);
		}
		return;
	case C_OUTPUT:
		{
			int _width, _height;
			wlr_output_effective_resolution(
					container->sway_output->wlr_output, &_width, &_height);
			width = container->width = _width;
			height = container->height = _height;
		}
		// arrange all workspaces:
		for (i = 0; i < container->children->length; ++i) {
			struct sway_container *child = container->children->items[i];
			arrange_windows(child, -1, -1);
		}
		return;
	case C_WORKSPACE:
		{
			struct sway_container *output =
				container_parent(container, C_OUTPUT);
			struct wlr_box *area = &output->sway_output->usable_area;
			wlr_log(L_DEBUG, "Usable area for ws: %dx%d@%d,%d",
					area->width, area->height, area->x, area->y);
			container->width = area->width;
			container->height = area->height;
			container->x = x = area->x;
			container->y = y = area->y;
			wlr_log(L_DEBUG, "Arranging workspace '%s' at %f, %f",
					container->name, container->x, container->y);
		}
		// children are properly handled below
		break;
	case C_VIEW:
		{
			container->width = width;
			container->height = height;
			view_set_size(container->sway_view,
				container->width, container->height);
			wlr_log(L_DEBUG, "Set view to %.f x %.f @ %.f, %.f",
					container->width, container->height,
					container->x, container->y);
		}
		return;
	default:
		container->width = width;
		container->height = height;
		x = container->x;
		y = container->y;
		break;
	}

	switch (container->layout) {
	case L_HORIZ:
		apply_horiz_layout(container, x, y, width, height, 0,
			container->children->length);
		break;
	case L_VERT:
		apply_vert_layout(container, x, y, width, height, 0,
			container->children->length);
		break;
	default:
		wlr_log(L_DEBUG, "TODO: arrange layout type %d", container->layout);
		apply_horiz_layout(container, x, y, width, height, 0,
			container->children->length);
		break;
	}
}

static void apply_horiz_layout(struct sway_container *container,
		const double x, const double y,
		const double width, const double height,
		const int start, const int end) {
	double scale = 0;
	// Calculate total width
	for (int i = start; i < end; ++i) {
		double *old_width =
			&((struct sway_container *)container->children->items[i])->width;
		if (*old_width <= 0) {
			if (end - start > 1) {
				*old_width = width / (end - start - 1);
			} else {
				*old_width = width;
			}
		}
		scale += *old_width;
	}
	scale = width / scale;

	// Resize windows
	double child_x = x;
	if (scale > 0.1) {
		wlr_log(L_DEBUG, "Arranging %p horizontally", container);
		for (int i = start; i < end; ++i) {
			struct sway_container *child = container->children->items[i];
			wlr_log(L_DEBUG,
				"Calculating arrangement for %p:%d (will scale %f by %f)",
				child, child->type, width, scale);
			view_set_position(child->sway_view, child_x, y);

			if (i == end - 1) {
				double remaining_width = x + width - child_x;
				arrange_windows(child, remaining_width, height);
			} else {
				arrange_windows(child, child->width * scale, height);
			}
			child_x += child->width;
		}

		// update focused view border last because it may
		// depend on the title bar geometry of its siblings.
		/* TODO WLR
		if (focused && container->children->length > 1) {
			update_container_border(focused);
		}
		*/
	}
}

void apply_vert_layout(struct sway_container *container,
		const double x, const double y,
		const double width, const double height, const int start,
		const int end) {
	int i;
	double scale = 0;
	// Calculate total height
	for (i = start; i < end; ++i) {
		double *old_height =
			&((struct sway_container *)container->children->items[i])->height;
		if (*old_height <= 0) {
			if (end - start > 1) {
				*old_height = height / (end - start - 1);
			} else {
				*old_height = height;
			}
		}
		scale += *old_height;
	}
	scale = height / scale;

	// Resize
	double child_y = y;
	if (scale > 0.1) {
		wlr_log(L_DEBUG, "Arranging %p vertically", container);
		for (i = start; i < end; ++i) {
			struct sway_container *child = container->children->items[i];
			wlr_log(L_DEBUG,
				"Calculating arrangement for %p:%d (will scale %f by %f)",
				child, child->type, height, scale);
			view_set_position(child->sway_view, x, child_y);

			if (i == end - 1) {
				double remaining_height = y + height - child_y;
				arrange_windows(child, width, remaining_height);
			} else {
				arrange_windows(child, width, child->height * scale);
			}
			child_y += child->height;
		}

		// update focused view border last because it may
		// depend on the title bar geometry of its siblings.
		/* TODO WLR
		if (focused && container->children->length > 1) {
			update_container_border(focused);
		}
		*/
	}
}

/**
 * Get swayc in the direction of newly entered output.
 */
static struct sway_container *get_swayc_in_output_direction(
		struct sway_container *output, enum movement_direction dir,
		struct sway_seat *seat) {
	if (!output) {
		return NULL;
	}

	struct sway_container *ws = seat_get_focus_inactive(seat, output);
	if (ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}

	if (ws == NULL) {
		wlr_log(L_ERROR, "got an output without a workspace");
		return NULL;
	}

	if (ws->children->length > 0) {
		switch (dir) {
		case MOVE_LEFT:
			// get most right child of new output
			return ws->children->items[ws->children->length-1];
		case MOVE_RIGHT:
			// get most left child of new output
			return ws->children->items[0];
		case MOVE_UP:
		case MOVE_DOWN: {
			struct sway_container *focused =
				seat_get_focus_inactive(seat, ws);
			if (focused && focused->parent) {
				struct sway_container *parent = focused->parent;
				if (parent->layout == L_VERT) {
					if (dir == MOVE_UP) {
						// get child furthest down on new output
						int idx = parent->children->length - 1;
						return parent->children->items[idx];
					} else if (dir == MOVE_DOWN) {
						// get child furthest up on new output
						return parent->children->items[0];
					}
				}
				return focused;
			}
			break;
		}
		default:
			break;
		}
	}

	return ws;
}

static void get_layout_center_position(struct sway_container *container,
		int *x, int *y) {
	// FIXME view coords are inconsistently referred to in layout/output systems
	if (container->type == C_OUTPUT) {
		*x = container->x + container->width/2;
		*y = container->y + container->height/2;
	} else {
		struct sway_container *output = container_parent(container, C_OUTPUT);
		if (container->type == C_WORKSPACE) {
			// Workspace coordinates are actually wrong/arbitrary, but should
			// be same as output.
			*x = output->x;
			*y = output->y;
		} else {
			*x = output->x + container->x;
			*y = output->y + container->y;
		}
	}
}

static bool sway_dir_to_wlr(enum movement_direction dir,
		enum wlr_direction *out) {
	switch (dir) {
	case MOVE_UP:
		*out = WLR_DIRECTION_UP;
		break;
	case MOVE_DOWN:
		*out = WLR_DIRECTION_DOWN;
		break;
	case MOVE_LEFT:
		*out = WLR_DIRECTION_LEFT;
		break;
	case MOVE_RIGHT:
		*out = WLR_DIRECTION_RIGHT;
		break;
	default:
		return false;
	}

	return true;
}

static struct sway_container *sway_output_from_wlr(struct wlr_output *output) {
	if (output == NULL) {
		return NULL;
	}
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *o = root_container.children->items[i];
		if (o->type == C_OUTPUT && o->sway_output->wlr_output == output) {
			return o;
		}
	}
	return NULL;
}

static struct sway_container *get_swayc_in_direction_under(
		struct sway_container *container, enum movement_direction dir,
		struct sway_seat *seat, struct sway_container *limit) {
	if (dir == MOVE_CHILD) {
		return seat_get_focus_inactive(seat, container);
	}

	struct sway_container *parent = container->parent;
	if (dir == MOVE_PARENT) {
		if (parent->type == C_OUTPUT) {
			return NULL;
		} else {
			return parent;
		}
	}

	if (dir == MOVE_PREV || dir == MOVE_NEXT) {
		int focused_idx = index_child(container);
		if (focused_idx == -1) {
			return NULL;
		} else {
			int desired = (focused_idx + (dir == MOVE_NEXT ? 1 : -1)) %
				parent->children->length;
			if (desired < 0) {
				desired += parent->children->length;
			}
			return parent->children->items[desired];
		}
	}

	// If moving to an adjacent output we need a starting position (since this
	// output might border to multiple outputs).
	//struct wlc_point abs_pos;
	//get_layout_center_position(container, &abs_pos);


	// TODO WLR fullscreen
	/*
	if (container->type == C_VIEW && swayc_is_fullscreen(container)) {
		wlr_log(L_DEBUG, "Moving from fullscreen view, skipping to output");
		container = container_parent(container, C_OUTPUT);
		get_layout_center_position(container, &abs_pos);
		struct sway_container *output =
		swayc_adjacent_output(container, dir, &abs_pos, true);
		return get_swayc_in_output_direction(output, dir);
	}
	if (container->type == C_WORKSPACE && container->fullscreen) {
		sway_log(L_DEBUG, "Moving to fullscreen view");
		return container->fullscreen;
	}
	*/

	struct sway_container *wrap_candidate = NULL;
	while (true) {
		// Test if we can even make a difference here
		bool can_move = false;
		int desired;
		int idx = index_child(container);
		if (parent->type == C_ROOT) {
			enum wlr_direction wlr_dir = 0;
			if (!sway_assert(sway_dir_to_wlr(dir, &wlr_dir),
						"got invalid direction: %d", dir)) {
				return NULL;
			}
			int lx, ly;
			get_layout_center_position(container, &lx, &ly);
			struct wlr_output_layout *layout =
				root_container.sway_root->output_layout;
			struct wlr_output *wlr_adjacent =
				wlr_output_layout_adjacent_output(layout, wlr_dir,
					container->sway_output->wlr_output, lx, ly);
			struct sway_container *adjacent =
				sway_output_from_wlr(wlr_adjacent);

			if (!adjacent || adjacent == container) {
				return wrap_candidate;
			}
			struct sway_container *next =
				get_swayc_in_output_direction(adjacent, dir, seat);
			if (next == NULL) {
				return NULL;
			}
			if (next->children && next->children->length) {
				// TODO consider floating children as well
				return seat_get_focus_inactive(seat, next);
			} else {
				return next;
			}
		} else {
			if (dir == MOVE_LEFT || dir == MOVE_RIGHT) {
				if (parent->layout == L_HORIZ || parent->layout == L_TABBED) {
					can_move = true;
					desired = idx + (dir == MOVE_LEFT ? -1 : 1);
				}
			} else {
				if (parent->layout == L_VERT || parent->layout == L_STACKED) {
					can_move = true;
					desired = idx + (dir == MOVE_UP ? -1 : 1);
				}
			}
		}

		if (can_move) {
			// TODO handle floating
			if (desired < 0 || desired >= parent->children->length) {
				can_move = false;
				int len = parent->children->length;
				if (!wrap_candidate && len > 1) {
					if (desired < 0) {
						wrap_candidate = parent->children->items[len-1];
					} else {
						wrap_candidate = parent->children->items[0];
					}
					if (config->force_focus_wrapping) {
						return wrap_candidate;
					}
				}
			} else {
				wlr_log(L_DEBUG,
					"cont %d-%p dir %i sibling %d: %p", idx,
					container, dir, desired, parent->children->items[desired]);
				return parent->children->items[desired];
			}
		}

		if (!can_move) {
			container = parent;
			parent = parent->parent;
			if (!parent || container == limit) {
				// wrapping is the last chance
				return wrap_candidate;
			}
		}
	}
}

struct sway_container *container_get_in_direction(
		struct sway_container *container, struct sway_seat *seat,
		enum movement_direction dir) {
	return get_swayc_in_direction_under(container, dir, seat, NULL);
}
