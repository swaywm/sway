#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"

static void apply_horiz_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}

	// Calculate total width of children
	double total_width = 0;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		if (child->width <= 0) {
			if (children->length > 1) {
				child->width = parent->width / (children->length - 1);
			} else {
				child->width = parent->width;
			}
		}
		container_remove_gaps(child);
		total_width += child->width;
	}
	double scale = parent->width / total_width;

	// Resize windows
	wlr_log(WLR_DEBUG, "Arranging %p horizontally", parent);
	double child_x = parent->x;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		child->x = child_x;
		child->y = parent->y;
		child->width = floor(child->width * scale);
		child->height = parent->height;
		child_x += child->width;

		// Make last child use remaining width of parent
		if (i == children->length - 1) {
			child->width = parent->x + parent->width - child->x;
		}
		container_add_gaps(child);
	}
}

static void apply_vert_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}

	// Calculate total height of children
	double total_height = 0;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		if (child->height <= 0) {
			if (children->length > 1) {
				child->height = parent->height / (children->length - 1);
			} else {
				child->height = parent->height;
			}
		}
		container_remove_gaps(child);
		total_height += child->height;
	}
	double scale = parent->height / total_height;

	// Resize
	wlr_log(WLR_DEBUG, "Arranging %p vertically", parent);
	double child_y = parent->y;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		child->x = parent->x;
		child->y = child_y;
		child->width = parent->width;
		child->height = floor(child->height * scale);
		child_y += child->height;

		// Make last child use remaining height of parent
		if (i == children->length - 1) {
			child->height = parent->y + parent->height - child->y;
		}
		container_add_gaps(child);
	}
}

static void apply_tabbed_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}
	size_t parent_offset = container_titlebar_height();
	size_t parent_height = parent->height - parent_offset;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		container_remove_gaps(child);
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent_height;
		container_add_gaps(child);
	}
}

static void apply_stacked_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}
	size_t parent_offset = container_titlebar_height() * children->length;
	size_t parent_height = parent->height - parent_offset;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		container_remove_gaps(child);
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent_height;
		container_add_gaps(child);
	}
}

static void arrange_floating(list_t *floating) {
	for (int i = 0; i < floating->length; ++i) {
		struct sway_container *floater = floating->items[i];
		arrange_container(floater);
	}
}

static void arrange_children(list_t *children,
		enum sway_container_layout layout, struct wlr_box *parent) {
	// Calculate x, y, width and height of children
	switch (layout) {
	case L_HORIZ:
		apply_horiz_layout(children, parent);
		break;
	case L_VERT:
		apply_vert_layout(children, parent);
		break;
	case L_TABBED:
		apply_tabbed_layout(children, parent);
		break;
	case L_STACKED:
		apply_stacked_layout(children, parent);
		break;
	case L_NONE:
		apply_horiz_layout(children, parent);
		break;
	}

	// Recurse into child containers
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		arrange_container(child);
	}
}

void arrange_container(struct sway_container *container) {
	if (config->reloading) {
		return;
	}
	if (container->type == C_VIEW) {
		view_autoconfigure(container->sway_view);
		container_set_dirty(container);
		return;
	}
	if (!sway_assert(container->type == C_CONTAINER, "Expected a container")) {
		return;
	}
	struct wlr_box box;
	container_get_box(container, &box);
	arrange_children(container->children, container->layout, &box);
	container_set_dirty(container);
}

void arrange_workspace(struct sway_container *workspace) {
	if (config->reloading) {
		return;
	}
	if (!sway_assert(workspace->type == C_WORKSPACE, "Expected a workspace")) {
		return;
	}
	struct sway_container *output = workspace->parent;
	struct wlr_box *area = &output->sway_output->usable_area;
	wlr_log(WLR_DEBUG, "Usable area for ws: %dx%d@%d,%d",
			area->width, area->height, area->x, area->y);
	workspace_remove_gaps(workspace);

	double prev_x = workspace->x;
	double prev_y = workspace->y;
	workspace->width = area->width;
	workspace->height = area->height;
	workspace->x = output->x + area->x;
	workspace->y = output->y + area->y;

	// Adjust any floating containers
	double diff_x = workspace->x - prev_x;
	double diff_y = workspace->y - prev_y;
	if (diff_x != 0 || diff_y != 0) {
		for (int i = 0; i < workspace->sway_workspace->floating->length; ++i) {
			struct sway_container *floater =
				workspace->sway_workspace->floating->items[i];
			container_floating_translate(floater, diff_x, diff_y);
			double center_x = floater->x + floater->width / 2;
			double center_y = floater->y + floater->height / 2;
			struct wlr_box workspace_box;
			container_get_box(workspace, &workspace_box);
			if (!wlr_box_contains_point(&workspace_box, center_x, center_y)) {
				container_floating_move_to_center(floater);
			}
		}
	}

	workspace_add_gaps(workspace);
	container_set_dirty(workspace);
	wlr_log(WLR_DEBUG, "Arranging workspace '%s' at %f, %f", workspace->name,
			workspace->x, workspace->y);
	if (workspace->sway_workspace->fullscreen) {
		struct sway_container *fs = workspace->sway_workspace->fullscreen;
		fs->x = workspace->parent->x;
		fs->y = workspace->parent->y;
		fs->width = workspace->parent->width;
		fs->height = workspace->parent->height;
		arrange_container(fs);
	} else {
		struct wlr_box box;
		container_get_box(workspace, &box);
		arrange_children(workspace->children, workspace->layout, &box);
		arrange_floating(workspace->sway_workspace->floating);
	}
}

void arrange_output(struct sway_container *output) {
	if (config->reloading) {
		return;
	}
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	const struct wlr_box *output_box = wlr_output_layout_get_box(
			root_container.sway_root->output_layout,
			output->sway_output->wlr_output);
	output->x = output_box->x;
	output->y = output_box->y;
	output->width = output_box->width;
	output->height = output_box->height;
	container_set_dirty(output);
	wlr_log(WLR_DEBUG, "Arranging output '%s' at %f,%f",
			output->name, output->x, output->y);
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		arrange_workspace(workspace);
	}
}

void arrange_root(void) {
	if (config->reloading) {
		return;
	}
	struct wlr_output_layout *output_layout =
		root_container.sway_root->output_layout;
	const struct wlr_box *layout_box =
		wlr_output_layout_get_box(output_layout, NULL);
	root_container.x = layout_box->x;
	root_container.y = layout_box->y;
	root_container.width = layout_box->width;
	root_container.height = layout_box->height;
	container_set_dirty(&root_container);
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		arrange_output(output);
	}
}

void arrange_windows(struct sway_container *container) {
	switch (container->type) {
	case C_ROOT:
		arrange_root();
		break;
	case C_OUTPUT:
		arrange_output(container);
		break;
	case C_WORKSPACE:
		arrange_workspace(container);
		break;
	case C_CONTAINER:
	case C_VIEW:
		arrange_container(container);
		break;
	case C_TYPES:
		break;
	}
}
