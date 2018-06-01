#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/debug.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"

struct sway_container root_container;

void arrange_root() {
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
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		arrange_output(output);
	}
}

void arrange_output(struct sway_container *output) {
	if (config->reloading) {
		return;
	}
	if (!sway_assert(output->type == C_OUTPUT,
			"called arrange_output() on non-output container")) {
		return;
	}
	const struct wlr_box *output_box = wlr_output_layout_get_box(
			root_container.sway_root->output_layout,
			output->sway_output->wlr_output);
	output->x = output_box->x;
	output->y = output_box->y;
	output->width = output_box->width;
	output->height = output_box->height;
	wlr_log(L_DEBUG, "Arranging output '%s' at %f,%f",
			output->name, output->x, output->y);
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		arrange_workspace(workspace);
	}
	container_damage_whole(output);
}

void arrange_workspace(struct sway_container *workspace) {
	if (config->reloading) {
		return;
	}
	if (!sway_assert(workspace->type == C_WORKSPACE,
			"called arrange_workspace() on non-workspace container")) {
		return;
	}
	struct sway_container *output = workspace->parent;
	struct wlr_box *area = &output->sway_output->usable_area;
	wlr_log(L_DEBUG, "Usable area for ws: %dx%d@%d,%d",
			area->width, area->height, area->x, area->y);
	workspace->width = area->width;
	workspace->height = area->height;
	workspace->x = output->x + area->x;
	workspace->y = output->y + area->y;
	wlr_log(L_DEBUG, "Arranging workspace '%s' at %f, %f",
			workspace->name, workspace->x, workspace->y);
	arrange_children_of(workspace);
	container_damage_whole(workspace);
}

static void apply_horiz_layout(struct sway_container *parent) {
	size_t num_children = parent->children->length;
	if (!num_children) {
		return;
	}
	size_t parent_offset = 0;
	if (parent->parent->layout == L_TABBED) {
		parent_offset = container_titlebar_height();
	} else if (parent->parent->layout == L_STACKED) {
		parent_offset =
			container_titlebar_height() * parent->parent->children->length;
	}
	size_t parent_height = parent->height - parent_offset;

	// Calculate total width of children
	double total_width = 0;
	for (size_t i = 0; i < num_children; ++i) {
		struct sway_container *child = parent->children->items[i];
		if (child->width <= 0) {
			if (num_children > 1) {
				child->width = parent->width / (num_children - 1);
			} else {
				child->width = parent->width;
			}
		}
		total_width += child->width;
	}
	double scale = parent->width / total_width;

	// Resize windows
	wlr_log(L_DEBUG, "Arranging %p horizontally", parent);
	double child_x = parent->x;
	struct sway_container *child;
	for (size_t i = 0; i < num_children; ++i) {
		child = parent->children->items[i];
		wlr_log(L_DEBUG,
				"Calculating arrangement for %p:%d (will scale %f by %f)",
				child, child->type, child->width, scale);
		child->x = child_x;
		child->y = parent->y + parent_offset;
		child->width = floor(child->width * scale);
		child->height = parent_height;
		child_x += child->width;
	}
	// Make last child use remaining width of parent
	child->width = parent->x + parent->width - child->x;
}

static void apply_vert_layout(struct sway_container *parent) {
	size_t num_children = parent->children->length;
	if (!num_children) {
		return;
	}
	size_t parent_offset = 0;
	if (parent->parent->layout == L_TABBED) {
		parent_offset = container_titlebar_height();
	} else if (parent->parent->layout == L_STACKED) {
		parent_offset =
			container_titlebar_height() * parent->parent->children->length;
	}
	size_t parent_height = parent->height - parent_offset;

	// Calculate total height of children
	double total_height = 0;
	for (size_t i = 0; i < num_children; ++i) {
		struct sway_container *child = parent->children->items[i];
		if (child->height <= 0) {
			if (num_children > 1) {
				child->height = parent_height / (num_children - 1);
			} else {
				child->height = parent_height;
			}
		}
		total_height += child->height;
	}
	double scale = parent_height / total_height;

	// Resize
	wlr_log(L_DEBUG, "Arranging %p vertically", parent);
	double child_y = parent->y + parent_offset;
	struct sway_container *child;
	for (size_t i = 0; i < num_children; ++i) {
		child = parent->children->items[i];
		wlr_log(L_DEBUG,
				"Calculating arrangement for %p:%d (will scale %f by %f)",
				child, child->type, child->height, scale);
		child->x = parent->x;
		child->y = child_y;
		child->width = parent->width;
		child->height = floor(child->height * scale);
		child_y += child->height;
	}
	// Make last child use remaining height of parent
	child->height = parent->y + parent_offset + parent_height - child->y;
}

static void apply_tabbed_or_stacked_layout(struct sway_container *parent) {
	if (!parent->children->length) {
		return;
	}
	size_t parent_offset = 0;
	if (parent->parent->layout == L_TABBED) {
		parent_offset = container_titlebar_height();
	} else if (parent->parent->layout == L_STACKED) {
		parent_offset =
			container_titlebar_height() * parent->parent->children->length;
	}
	size_t parent_height = parent->height - parent_offset;
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent_height;
	}
}

void arrange_children_of(struct sway_container *parent) {
	if (config->reloading) {
		return;
	}
	if (!sway_assert(parent->type == C_WORKSPACE || parent->type == C_CONTAINER,
			"container is a %s", container_type_to_str(parent->type))) {
		return;
	}

	struct sway_container *workspace = parent;
	if (workspace->type != C_WORKSPACE) {
		workspace = container_parent(workspace, C_WORKSPACE);
	}
	if (workspace->sway_workspace->fullscreen) {
		// Just arrange the fullscreen view and jump out
		view_autoconfigure(workspace->sway_workspace->fullscreen);
		return;
	}

	wlr_log(L_DEBUG, "Arranging layout for %p %s %fx%f+%f,%f", parent,
		parent->name, parent->width, parent->height, parent->x, parent->y);

	// Calculate x, y, width and height of children
	switch (parent->layout) {
	case L_HORIZ:
		apply_horiz_layout(parent);
		break;
	case L_VERT:
		apply_vert_layout(parent);
		break;
	case L_TABBED:
	case L_STACKED:
		apply_tabbed_or_stacked_layout(parent);
		break;
	default:
		wlr_log(L_DEBUG, "TODO: arrange layout type %d", parent->layout);
		apply_horiz_layout(parent);
		break;
	}

	// Apply x, y, width and height to children and recurse if needed
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		if (child->type == C_VIEW) {
			view_autoconfigure(child->sway_view);
		} else {
			arrange_children_of(child);
		}
	}

	// If container is a workspace, process floating containers too
	if (parent->type == C_WORKSPACE) {
		struct sway_workspace *ws = workspace->sway_workspace;
		for (int i = 0; i < ws->floating->children->length; ++i) {
			struct sway_container *child = ws->floating->children->items[i];
			if (child->type != C_VIEW) {
				arrange_children_of(child);
			}
		}
	}

	container_damage_whole(parent);
	update_debug_tree();
}
