#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/debug.h"
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

static void output_layout_handle_change(struct wl_listener *listener,
		void *data) {
	struct wlr_output_layout *output_layout =
		root_container.sway_root->output_layout;
	const struct wlr_box *layout_box =
		wlr_output_layout_get_box(output_layout, NULL);
	root_container.x = layout_box->x;
	root_container.y = layout_box->y;
	root_container.width = layout_box->width;
	root_container.height = layout_box->height;

	for (int i = 0 ; i < root_container.children->length; ++i) {
		struct sway_container *output_container =
			root_container.children->items[i];
		if (output_container->type != C_OUTPUT) {
			continue;
		}
		struct sway_output *output = output_container->sway_output;

		const struct wlr_box *output_box =
			wlr_output_layout_get_box(output_layout, output->wlr_output);
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

struct sway_container *container_set_layout(struct sway_container *container,
		enum sway_container_layout layout) {
	if (container->type == C_WORKSPACE) {
		container->workspace_layout = layout;
		if (layout == L_HORIZ || layout == L_VERT) {
			container->layout = layout;
		}
	} else {
		container->layout = layout;
	}
	return container;
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
	wl_list_init(&root_container.sway_root->xwayland_unmanaged);
	wl_signal_init(&root_container.sway_root->events.new_container);

	root_container.sway_root->output_layout_change.notify =
		output_layout_handle_change;
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

void container_insert_child(struct sway_container *parent,
		struct sway_container *child, int i) {
	struct sway_container *old_parent = child->parent;
	if (old_parent) {
		container_remove_child(child);
	}
	wlr_log(L_DEBUG, "Inserting id:%zd at index %d", child->id, i);
	list_insert(parent->children, i, child);
	child->parent = parent;
	wl_signal_emit(&child->events.reparent, old_parent);
}

struct sway_container *container_add_sibling(struct sway_container *fixed,
		struct sway_container *active) {
	// TODO handle floating
	struct sway_container *old_parent = NULL;
	if (active->parent) {
		old_parent = active->parent;
		container_remove_child(active);
	}
	struct sway_container *parent = fixed->parent;
	int i = index_child(fixed);
	list_insert(parent->children, i + 1, active);
	active->parent = parent;
	wl_signal_emit(&active->events.reparent, old_parent);
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

struct sway_container *container_remove_child(struct sway_container *child) {
	struct sway_container *parent = child->parent;
	for (int i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			list_del(parent->children, i);
			break;
		}
	}
	child->parent = NULL;
	return parent;
}

void container_move_to(struct sway_container *container,
		struct sway_container *destination) {
	if (container == destination
			|| container_has_anscestor(container, destination)) {
		return;
	}
	struct sway_container *old_parent = container_remove_child(container);
	container->width = container->height = 0;
	struct sway_container *new_parent;
	if (destination->type == C_VIEW) {
		new_parent = container_add_sibling(destination, container);
	} else {
		new_parent = destination;
		container_add_child(destination, container);
	}
	wl_signal_emit(&container->events.reparent, old_parent);
	if (container->type == C_WORKSPACE) {
		struct sway_seat *seat = input_manager_get_default_seat(
				input_manager);
		if (old_parent->children->length == 0) {
			char *ws_name = workspace_next_name(old_parent->name);
			struct sway_container *ws =
				workspace_create(old_parent, ws_name);
			free(ws_name);
			seat_set_focus(seat, ws);
		}
		container_sort_workspaces(new_parent);
		seat_set_focus(seat, new_parent);
	}
	if (old_parent) {
		arrange_windows(old_parent, -1, -1);
	}
	arrange_windows(new_parent, -1, -1);
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

static bool is_parallel(enum sway_container_layout layout,
		enum movement_direction dir) {
	switch (layout) {
	case L_TABBED:
	case L_STACKED:
	case L_HORIZ:
		return dir == MOVE_LEFT || dir == MOVE_RIGHT;
	case L_VERT:
		return dir == MOVE_UP || dir == MOVE_DOWN;
	default:
		return false;
	}
}

static enum movement_direction invert_movement(enum movement_direction dir) {
	switch (dir) {
	case MOVE_LEFT:
		return MOVE_RIGHT;
	case MOVE_RIGHT:
		return MOVE_LEFT;
	case MOVE_UP:
		return MOVE_DOWN;
	case MOVE_DOWN:
		return MOVE_UP;
	default:
		sway_assert(0, "This function expects left|right|up|down");
		return MOVE_LEFT;
	}
}

static int move_offs(enum movement_direction move_dir) {
	return move_dir == MOVE_LEFT || move_dir == MOVE_UP ? -1 : 1;
}

/* Gets the index of the most extreme member based on the movement offset */
static int container_limit(struct sway_container *container,
		enum movement_direction move_dir) {
	return move_offs(move_dir) < 0 ? 0 : container->children->length;
}

/* Takes one child, sets it aside, wraps the rest of the children in a new
 * container, switches the layout of the workspace, and drops the child back in.
 * In other words, rejigger it. */
static void workspace_rejigger(struct sway_container *ws,
		struct sway_container *child, enum movement_direction move_dir) {
	struct sway_container *original_parent = child->parent;
	struct sway_container *new_parent =
		container_split(ws, ws->layout);

	container_remove_child(child);
	for (int i = 0; i < ws->children->length; ++i) {
		struct sway_container *_child = ws->children->items[i];
		container_move_to(new_parent, _child);
	}

	int index = move_offs(move_dir);
	container_insert_child(ws, child, index < 0 ? 0 : 1);
	container_set_layout(ws,
			move_dir == MOVE_LEFT || move_dir == MOVE_RIGHT ? L_HORIZ : L_VERT);

	container_flatten(ws);
	container_reap_empty_recursive(original_parent);
	wl_signal_emit(&child->events.reparent, original_parent);
	arrange_windows(ws, -1, -1);
}

void container_move(struct sway_container *container,
		enum movement_direction move_dir, int move_amt) {
	if (!sway_assert(
				container->type != C_CONTAINER || container->type != C_VIEW,
				"Can only move containers and views")) {
		return;
	}
	int offs = move_offs(move_dir);

	struct sway_container *sibling = NULL;
	struct sway_container *current = container;
	struct sway_container *parent = current->parent;

	if (parent != container_flatten(parent)) {
		// Special case: we were the last one in this container, so flatten it
		// and leave
		update_debug_tree();
		return;
	}

	while (!sibling) {
		if (current->type == C_ROOT) {
			return;
		}

		parent = current->parent;
		wlr_log(L_DEBUG, "Visiting %p %s '%s'", current,
				container_type_to_str(current->type), current->name);

		int index = index_child(current);

		switch (current->type) {
		case C_OUTPUT: {
			enum wlr_direction wlr_dir;
			sway_dir_to_wlr(move_dir, &wlr_dir);
			double ref_lx = current->x + current->width / 2;
			double ref_ly = current->y + current->height / 2;
			struct wlr_output *next = wlr_output_layout_adjacent_output(
				root_container.sway_root->output_layout, wlr_dir,
				current->sway_output->wlr_output, ref_lx, ref_ly);
			if (!next) {
				wlr_log(L_DEBUG, "Hit edge of output, nowhere else to go");
				return;
			}
			struct sway_output *next_output = next->data;
			current = next_output->swayc;
			wlr_log(L_DEBUG, "Selected next output (%s)", current->name);
			// Select workspace and get outta here
			current = seat_get_focus_inactive(
					config->handler_context.seat, current);
			if (current->type != C_WORKSPACE) {
				current = container_parent(current, C_WORKSPACE);
			}
			sibling = current;
			break;
		}
		case C_WORKSPACE:
			if (!is_parallel(current->layout, move_dir)) {
				if (current->children->length != 1) {
					// Special case
					wlr_log(L_DEBUG, "Rejiggering the workspace (%d kiddos)",
							current->children->length);
					workspace_rejigger(current, container, move_dir);
				}
				return;
			} else {
				wlr_log(L_DEBUG, "Selecting output");
				current = current->parent;
			}
			break;
		case C_CONTAINER:
		case C_VIEW:
			if (is_parallel(parent->layout, move_dir)) {
				if ((index == parent->children->length - 1 && offs > 0)
						|| (index == 0 && offs < 0)) {
					if (current->parent == container->parent) {
						wlr_log(L_DEBUG, "Hit limit, selecting parent");
						current = current->parent;
					} else {
						wlr_log(L_DEBUG, "Hit limit, "
								"promoting descendant to sibling");
						// Special case
						struct sway_container *old_parent = container->parent;
						container_insert_child(current->parent, container,
								index + (offs < 0 ? 0 : 1));
						container->width = container->height = 0;
						arrange_windows(current->parent, -1, -1);
						arrange_windows(old_parent, -1, -1);
						return;
					}
				} else {
					sibling = parent->children->items[index + offs];
					wlr_log(L_DEBUG, "Selecting sibling id:%zd", sibling->id);
				}
			} else {
				wlr_log(L_DEBUG, "Moving up to find a parallel container");
				current = current->parent;
			}
			break;
		default:
			sway_assert(0, "Not expecting to see container of type %s here",
					container_type_to_str(current->type));
			return;
		}
	}

	// Part two: move stuff around
	int index = index_child(container);
	struct sway_container *old_parent = container->parent;

	while (sibling) {
		switch (sibling->type) {
		case C_VIEW:
			if (sibling->parent == container->parent) {
				wlr_log(L_DEBUG, "Swapping siblings");
				sibling->parent->children->items[index + offs] = container;
				sibling->parent->children->items[index] = sibling;
				arrange_windows(sibling->parent, -1, -1);
			} else {
				wlr_log(L_DEBUG, "Promoting to sibling of cousin");
				container_insert_child(sibling->parent, container,
						index_child(sibling) + (offs > 0 ? 0 : 1));
				container->width = container->height = 0;
				arrange_windows(sibling->parent, -1, -1);
				arrange_windows(old_parent, -1, -1);
			}
			sibling = NULL;
			break;
		case C_WORKSPACE: // Note: only in the case of moving between outputs
		case C_CONTAINER:
			if (is_parallel(sibling->layout, move_dir)) {
				int limit = container_limit(sibling, invert_movement(move_dir));
				wlr_log(L_DEBUG, "limit: %d", limit);
				wlr_log(L_DEBUG,
						"Reparenting container (parallel) to index %d "
						"(move dir: %d)", limit, move_dir);
				container_insert_child(sibling, container, limit);
				container->width = container->height = 0;
				arrange_windows(sibling, -1, -1);
				arrange_windows(old_parent, -1, -1);
				sibling = NULL;
			} else {
				wlr_log(L_DEBUG, "Reparenting container (perpendicular)");
				container_remove_child(container);
				struct sway_container *focus_inactive = seat_get_focus_inactive(
						config->handler_context.seat, sibling);
				if (focus_inactive) {
					while (focus_inactive->parent != sibling) {
						focus_inactive = focus_inactive->parent;
					}
					wlr_log(L_DEBUG, "Focus inactive: id:%zd",
							focus_inactive->id);
					sibling = focus_inactive;
					continue;
				} else if (sibling->children->length) {
					wlr_log(L_DEBUG, "No focus-inactive, adding arbitrarily");
					container_add_sibling(sibling->children->items[0], container);
				} else {
					wlr_log(L_DEBUG, "No kiddos, adding container alone");
					container_add_child(sibling, container);
				}
				container->width = container->height = 0;
				arrange_windows(sibling, -1, -1);
				arrange_windows(old_parent, -1, -1);
				sibling = NULL;
			}
			break;
		default:
			sway_assert(0, "Not expecting to see container of type %s here",
					container_type_to_str(sibling->type));
			return;
		}
	}

	if (old_parent) {
		seat_set_focus(config->handler_context.seat, old_parent);
		seat_set_focus(config->handler_context.seat, container);
	}

	struct sway_container *last_ws = old_parent;
	struct sway_container *next_ws = container->parent;
	if (last_ws && last_ws->type != C_WORKSPACE) {
		last_ws = container_parent(last_ws, C_WORKSPACE);
	}
	if (next_ws && next_ws->type != C_WORKSPACE) {
		next_ws = container_parent(next_ws, C_WORKSPACE);
	}
	if (last_ws && next_ws && last_ws != next_ws) {
		ipc_event_workspace(last_ws, container, "focus");
	}
}

enum sway_container_layout container_get_default_layout(
		struct sway_container *con) {
	if (con->type != C_OUTPUT) {
		con = container_parent(con, C_OUTPUT);
	}

	if (!sway_assert(con != NULL,
			"container_get_default_layout must be called on an attached"
			" container below the root container")) {
		return 0;
	}

	if (config->default_layout != L_NONE) {
		return config->default_layout;
	} else if (config->default_orientation != L_NONE) {
		return config->default_orientation;
	} else if (con->width >= con->height) {
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
	if (config->reloading) {
		return;
	}
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
			container->width = width = area->width;
			container->height = height = area->height;
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
			view_configure(container->sway_view, container->x, container->y,
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
	container_damage_whole(container);
	// TODO: Make this less shitty
	update_debug_tree();
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

			if (child->type == C_VIEW) {
				view_configure(child->sway_view, child_x, y, child->width,
					child->height);
			} else {
				child->x = child_x;
				child->y = y;
			}

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
			if (child->type == C_VIEW) {
				view_configure(child->sway_view, x, child_y, child->width,
					child->height);
			} else {
				child->x = x;
				child->y = child_y;
			}

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

struct sway_container *container_get_in_direction(
		struct sway_container *container, struct sway_seat *seat,
		enum movement_direction dir) {
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
				return seat_get_focus_by_type(seat, next, C_VIEW);
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
				struct sway_container *desired_con = parent->children->items[desired];
				wlr_log(L_DEBUG,
					"cont %d-%p dir %i sibling %d: %p", idx,
					container, dir, desired, desired_con);
				struct sway_container *next = seat_get_focus_by_type(seat, desired_con, C_VIEW);
				return next;
			}
		}

		if (!can_move) {
			container = parent;
			parent = parent->parent;
			if (!parent) {
				// wrapping is the last chance
				return wrap_candidate;
			}
		}
	}
}

struct sway_container *container_replace_child(struct sway_container *child,
		struct sway_container *new_child) {
	struct sway_container *parent = child->parent;
	if (parent == NULL) {
		return NULL;
	}
	int i = index_child(child);

	// TODO floating
	if (new_child->parent) {
		container_remove_child(new_child);
	}
	parent->children->items[i] = new_child;
	new_child->parent = parent;
	child->parent = NULL;

	// Set geometry for new child
	new_child->x = child->x;
	new_child->y = child->y;
	new_child->width = child->width;
	new_child->height = child->height;

	// reset geometry for child
	child->width = 0;
	child->height = 0;

	return parent;
}

struct sway_container *container_split(struct sway_container *child,
		enum sway_container_layout layout) {
	// TODO floating: cannot split a floating container
	if (!sway_assert(child, "child cannot be null")) {
		return NULL;
	}
	if (child->type == C_WORKSPACE && child->children->length == 0) {
		// Special case: this just behaves like splitt
		child->prev_layout = child->layout;
		child->layout = layout;
		arrange_windows(child, -1, -1);
		return child;
	}

	struct sway_container *cont = container_create(C_CONTAINER);

	wlr_log(L_DEBUG, "creating container %p around %p", cont, child);

	cont->prev_layout = L_NONE;
	cont->width = child->width;
	cont->height = child->height;
	cont->x = child->x;
	cont->y = child->y;

	if (child->type == C_WORKSPACE) {
		struct sway_seat *seat = input_manager_get_default_seat(input_manager);
		struct sway_container *workspace = child;
		bool set_focus = (seat_get_focus(seat) == workspace);

		while (workspace->children->length) {
			struct sway_container *ws_child = workspace->children->items[0];
			container_remove_child(ws_child);
			container_add_child(cont, ws_child);
		}

		container_add_child(workspace, cont);
		enum sway_container_layout old_layout = workspace->layout;
		container_set_layout(workspace, layout);
		cont->layout = old_layout;

		if (set_focus) {
			seat_set_focus(seat, cont);
		}
	} else {
		cont->layout = layout;
		container_replace_child(child, cont);
		container_add_child(cont, child);
	}

	return cont;
}

void container_recursive_resize(struct sway_container *container,
		double amount, enum resize_edge edge) {
	bool layout_match = true;
	wlr_log(L_DEBUG, "Resizing %p with amount: %f", container, amount);
	if (edge == RESIZE_EDGE_LEFT || edge == RESIZE_EDGE_RIGHT) {
		container->width += amount;
		layout_match = container->layout == L_HORIZ;
	} else if (edge == RESIZE_EDGE_TOP || edge == RESIZE_EDGE_BOTTOM) {
		container->height += amount;
		layout_match = container->layout == L_VERT;
	}
	if (container->children) {
		for (int i = 0; i < container->children->length; i++) {
			struct sway_container *child = container->children->items[i];
			double amt = layout_match ?
				amount / container->children->length : amount;
			container_recursive_resize(child, amt, edge);
		}
	}
}
