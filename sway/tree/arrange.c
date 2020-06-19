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

	// Count the number of new windows we are resizing, and how much space
	// is currently occupied
	int new_children = 0;
	double current_width_fraction = 0;
	struct sway_container *child;
	list_for_each(child, children) {
		current_width_fraction += child->width_fraction;
		if (child->width_fraction <= 0) {
			new_children += 1;
		}
	}

	// Calculate each height fraction
	double total_width_fraction = 0;
	list_for_each(child, children) {
		if (child->width_fraction <= 0) {
			if (current_width_fraction <= 0) {
				child->width_fraction = 1.0;
			} else if (children->length > new_children) {
				child->width_fraction = current_width_fraction /
					(children->length - new_children);
			} else {
				child->width_fraction = current_width_fraction;
			}
		}
		total_width_fraction += child->width_fraction;
	}
	// Normalize width fractions so the sum is 1.0
	list_for_each(child, children) {
		child->width_fraction /= total_width_fraction;
	}

	// Calculate gap size
	double inner_gap = 0;
	child = children->items[0];
	struct sway_workspace *ws = child->workspace;
	if (ws) {
		inner_gap = ws->gaps_inner;
	}
	// Descendants of tabbed/stacked containers don't have gaps
	struct sway_container *temp = child;
	while (temp) {
		enum sway_container_layout layout = container_parent_layout(temp);
		if (layout == L_TABBED || layout == L_STACKED) {
			inner_gap = 0;
		}
		temp = temp->parent;
	}
	double total_gap = fmin(inner_gap * (children->length - 1),
		fmax(0, parent->width - MIN_SANE_W * children->length));
	double child_total_width = parent->width - total_gap;
	inner_gap = floor(total_gap / (children->length - 1));

	// Resize windows
	sway_log(SWAY_DEBUG, "Arranging %p horizontally", parent);
	double child_x = parent->x;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		child->child_total_width = child_total_width;
		child->x = child_x;
		child->y = parent->y;
		child->width = round(child->width_fraction * child_total_width);
		child->height = parent->height;
		child_x += child->width + inner_gap;

		// Make last child use remaining width of parent
		if (i == children->length - 1) {
			child->width = parent->x + parent->width - child->x;
		}
	}
}

static void apply_vert_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}

	// Count the number of new windows we are resizing, and how much space
	// is currently occupied
	int new_children = 0;
	double current_height_fraction = 0;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		current_height_fraction += child->height_fraction;
		if (child->height_fraction <= 0) {
			new_children += 1;
		}
	}

	// Calculate each height fraction
	double total_height_fraction = 0;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		if (child->height_fraction <= 0) {
			if (current_height_fraction <= 0) {
				child->height_fraction = 1.0;
			} else if (children->length > new_children) {
				child->height_fraction = current_height_fraction /
					(children->length - new_children);
			} else {
				child->height_fraction = current_height_fraction;
			}
		}
		total_height_fraction += child->height_fraction;
	}
	// Normalize height fractions so the sum is 1.0
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		child->height_fraction /= total_height_fraction;
	}

	// Calculate gap size
	double inner_gap = 0;
	struct sway_container *child = children->items[0];
	struct sway_workspace *ws = child->workspace;
	if (ws) {
		inner_gap = ws->gaps_inner;
	}
	// Descendants of tabbed/stacked containers don't have gaps
	struct sway_container *temp = child;
	while (temp) {
		enum sway_container_layout layout = container_parent_layout(temp);
		if (layout == L_TABBED || layout == L_STACKED) {
			inner_gap = 0;
		}
		temp = temp->parent;
	}
	double total_gap = fmin(inner_gap * (children->length - 1),
		fmax(0, parent->height - MIN_SANE_H * children->length));
	double child_total_height = parent->height - total_gap;
	inner_gap = floor(total_gap / (children->length - 1));

	// Resize windows
	sway_log(SWAY_DEBUG, "Arranging %p vertically", parent);
	double child_y = parent->y;
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		child->child_total_height = child_total_height;
		child->x = parent->x;
		child->y = child_y;
		child->width = parent->width;
		child->height = round(child->height_fraction * child_total_height);
		child_y += child->height + inner_gap;

		// Make last child use remaining height of parent
		if (i == children->length - 1) {
			child->height = parent->y + parent->height - child->y;
		}
	}
}

static void apply_tabbed_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		int parent_offset = child->view ? 0 : container_titlebar_height();
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent->height - parent_offset;
	}
}

static void apply_stacked_layout(list_t *children, struct wlr_box *parent) {
	if (!children->length) {
		return;
	}
	struct sway_container *child;
	list_for_each(child, children) {
		int parent_offset = child->view ?  0 :
			container_titlebar_height() * children->length;
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent->height - parent_offset;
	}
}

static void arrange_floating(list_t *floating) {
	struct sway_container *floater;
	list_for_each(floater, floating) {
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
	struct sway_container *child;
	list_for_each(child, children) {
		arrange_container(child);
	}
}

void arrange_container(struct sway_container *container) {
	if (config->reloading) {
		return;
	}
	if (container->view) {
		view_autoconfigure(container->view);
		node_set_dirty(&container->node);
		return;
	}
	struct wlr_box box;
	container_get_box(container, &box);
	arrange_children(container->children, container->layout, &box);
	node_set_dirty(&container->node);
}

void arrange_workspace(struct sway_workspace *workspace) {
	if (config->reloading) {
		return;
	}
	if (!workspace->output) {
		// Happens when there are no outputs connected
		return;
	}
	struct sway_output *output = workspace->output;
	struct wlr_box *area = &output->usable_area;
	sway_log(SWAY_DEBUG, "Usable area for ws: %dx%d@%d,%d",
			area->width, area->height, area->x, area->y);

	bool first_arrange = workspace->width == 0 && workspace->height == 0;
	double prev_x = workspace->x - workspace->current_gaps.left;
	double prev_y = workspace->y - workspace->current_gaps.top;
	workspace->width = area->width;
	workspace->height = area->height;
	workspace->x = output->lx + area->x;
	workspace->y = output->ly + area->y;

	// Adjust any floating containers
	double diff_x = workspace->x - prev_x;
	double diff_y = workspace->y - prev_y;
	if (!first_arrange && (diff_x != 0 || diff_y != 0)) {
		struct sway_container *floater;
		list_for_each(floater, workspace->floating) {
			container_floating_translate(floater, diff_x, diff_y);
			double center_x = floater->x + floater->width / 2;
			double center_y = floater->y + floater->height / 2;
			struct wlr_box workspace_box;
			workspace_get_box(workspace, &workspace_box);
			if (!wlr_box_contains_point(&workspace_box, center_x, center_y)) {
				container_floating_move_to_center(floater);
			}
		}
	}

	workspace_add_gaps(workspace);
	node_set_dirty(&workspace->node);
	sway_log(SWAY_DEBUG, "Arranging workspace '%s' at %f, %f", workspace->name,
			workspace->x, workspace->y);
	if (workspace->fullscreen) {
		struct sway_container *fs = workspace->fullscreen;
		fs->x = output->lx;
		fs->y = output->ly;
		fs->width = output->width;
		fs->height = output->height;
		arrange_container(fs);
	} else {
		struct wlr_box box;
		workspace_get_box(workspace, &box);
		arrange_children(workspace->tiling, workspace->layout, &box);
		arrange_floating(workspace->floating);
	}
}

void arrange_output(struct sway_output *output) {
	if (config->reloading) {
		return;
	}
	const struct wlr_box *output_box = wlr_output_layout_get_box(
			root->output_layout, output->wlr_output);
	output->lx = output_box->x;
	output->ly = output_box->y;
	output->width = output_box->width;
	output->height = output_box->height;

	struct sway_workspace *workspace;
	list_for_each(workspace, output->workspaces) {
		arrange_workspace(workspace);
	}
}

void arrange_root(void) {
	if (config->reloading) {
		return;
	}
	const struct wlr_box *layout_box =
		wlr_output_layout_get_box(root->output_layout, NULL);
	root->x = layout_box->x;
	root->y = layout_box->y;
	root->width = layout_box->width;
	root->height = layout_box->height;

	if (root->fullscreen_global) {
		struct sway_container *fs = root->fullscreen_global;
		fs->x = root->x;
		fs->y = root->y;
		fs->width = root->width;
		fs->height = root->height;
		arrange_container(fs);
	} else {
		struct sway_output *output;
		list_for_each(output, root->outputs) {
			arrange_output(output);
		}
	}
}

void arrange_node(struct sway_node *node) {
	switch (node->type) {
	case N_ROOT:
		arrange_root();
		break;
	case N_OUTPUT:
		arrange_output(node->sway_output);
		break;
	case N_WORKSPACE:
		arrange_workspace(node->sway_workspace);
		break;
	case N_CONTAINER:
		arrange_container(node->sway_container);
		break;
	}
}
