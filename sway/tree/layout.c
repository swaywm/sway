#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"
#include "sway/view.h"
#include "list.h"
#include "log.h"

swayc_t root_container;

static void output_layout_change_notify(struct wl_listener *listener, void *data) {
	struct wlr_box *box = wlr_output_layout_get_box(
		root_container.sway_root->output_layout, NULL);
	root_container.width = box->width;
	root_container.height = box->height;
}

void init_layout(void) {
	root_container.id = 0; // normally assigned in new_swayc()
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.name = strdup("root");
	root_container.children = create_list();

	root_container.sway_root = calloc(1, sizeof(*root_container.sway_root));
	root_container.sway_root->output_layout = wlr_output_layout_create();

	root_container.sway_root->output_layout_change.notify =
		output_layout_change_notify;
	wl_signal_add(&root_container.sway_root->output_layout->events.change,
		&root_container.sway_root->output_layout_change);
}

void add_child(swayc_t *parent, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %fx%f) to %p (%d, %fx%f)",
			child, child->type, child->width, child->height,
			parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
	child->parent = parent;
	// set focus for this container
	if (!parent->focused) {
		parent->focused = child;
	}
	/* TODO WLR
	if (parent->type == C_WORKSPACE && child->type == C_VIEW && (parent->workspace_layout == L_TABBED || parent->workspace_layout == L_STACKED)) {
		child = new_container(child, parent->workspace_layout);
	}
	*/
}

swayc_t *remove_child(swayc_t *child) {
	int i;
	swayc_t *parent = child->parent;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			list_del(parent->children, i);
			break;
		}
	}
	child->parent = NULL;
	return parent;
}

enum swayc_layouts default_layout(swayc_t *output) {
	/* TODO WLR
	if (config->default_layout != L_NONE) {
		//return config->default_layout;
	} else if (config->default_orientation != L_NONE) {
		return config->default_orientation;
	} else */if (output->width >= output->height) {
		return L_HORIZ;
	} else {
		return L_VERT;
	}
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	swayc_t *a = *(void **)_a;
	swayc_t *b = *(void **)_b;
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

void sort_workspaces(swayc_t *output) {
	list_stable_sort(output->children, sort_workspace_cmp_qsort);
}

static void apply_horiz_layout(swayc_t *container, const double x,
				const double y, const double width,
				const double height, const int start,
				const int end);

static void apply_vert_layout(swayc_t *container, const double x,
				const double y, const double width,
				const double height, const int start,
				const int end);

void arrange_windows(swayc_t *container, double width, double height) {
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

	sway_log(L_DEBUG, "Arranging layout for %p %s %fx%f+%f,%f", container,
		 container->name, container->width, container->height, container->x,
		 container->y);

	double x = 0, y = 0;
	switch (container->type) {
	case C_ROOT:
		// TODO: wlr_output_layout probably
		for (i = 0; i < container->children->length; ++i) {
			swayc_t *output = container->children->items[i];
			sway_log(L_DEBUG, "Arranging output '%s' at %f,%f",
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
			swayc_t *child = container->children->items[i];
			arrange_windows(child, -1, -1);
		}
		return;
	case C_WORKSPACE:
		{
			swayc_t *output = swayc_parent_by_type(container, C_OUTPUT);
			container->width = output->width;
			container->height = output->height;
			container->x = x;
			container->y = y;
			sway_log(L_DEBUG, "Arranging workspace '%s' at %f, %f",
					container->name, container->x, container->y);
		}
		// children are properly handled below
		break;
	case C_VIEW:
		{
			container->width = width;
			container->height = height;
			container->sway_view->iface.set_size(container->sway_view,
					container->width, container->height);
			sway_log(L_DEBUG, "Set view to %.f x %.f @ %.f, %.f",
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
		sway_log(L_DEBUG, "TODO: arrange layout type %d", container->layout);
		apply_horiz_layout(container, x, y, width, height, 0,
			container->children->length);
		break;
	}
}

static void apply_horiz_layout(swayc_t *container,
		const double x, const double y,
		const double width, const double height,
		const int start, const int end) {
	double scale = 0;
	// Calculate total width
	for (int i = start; i < end; ++i) {
		double *old_width = &((swayc_t *)container->children->items[i])->width;
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
		sway_log(L_DEBUG, "Arranging %p horizontally", container);
		for (int i = start; i < end; ++i) {
			swayc_t *child = container->children->items[i];
			sway_log(L_DEBUG,
				 "Calculating arrangement for %p:%d (will scale %f by %f)",
				 child, child->type, width, scale);
			child->sway_view->iface.set_position(child->sway_view, child_x, y);

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

void apply_vert_layout(swayc_t *container,
		const double x, const double y,
		const double width, const double height, const int start,
		const int end) {
	int i;
	double scale = 0;
	// Calculate total height
	for (i = start; i < end; ++i) {
		double *old_height = &((swayc_t *)container->children->items[i])->height;
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
		sway_log(L_DEBUG, "Arranging %p vertically", container);
		for (i = start; i < end; ++i) {
			swayc_t *child = container->children->items[i];
			sway_log(L_DEBUG,
				 "Calculating arrangement for %p:%d (will scale %f by %f)",
				 child, child->type, height, scale);
			child->sway_view->iface.set_position(child->sway_view, x, child_y);

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
