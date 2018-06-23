#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"

struct sway_container root_container;

static void apply_horiz_layout(struct sway_container *parent) {
	size_t num_children = parent->children->length;
	if (!num_children) {
		return;
	}
	size_t parent_offset = 0;
	if (parent->parent->layout == L_TABBED) {
		parent_offset = container_titlebar_height();
	} else if (parent->parent->layout == L_STACKED) {
		parent_offset = container_titlebar_height() *
			parent->parent->children->length;
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
		remove_gaps(child);
		total_width += child->width;
	}
	double scale = parent->width / total_width;

	// Resize windows
	wlr_log(L_DEBUG, "Arranging %p horizontally", parent);
	double child_x = parent->x;
	for (size_t i = 0; i < num_children; ++i) {
		struct sway_container *child = parent->children->items[i];
		wlr_log(L_DEBUG,
				"Calculating arrangement for %p:%d (will scale %f by %f)",
				child, child->type, child->width, scale);
		child->x = child_x;
		child->y = parent->y + parent_offset;
		child->width = floor(child->width * scale);
		child->height = parent_height;
		child_x += child->width;

		// Make last child use remaining width of parent
		if (i == num_children - 1) {
			child->width = parent->x + parent->width - child->x;
		}
		add_gaps(child);
	}
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
	size_t parent_height = parent->height + parent_offset;

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
		remove_gaps(child);
		total_height += child->height;
	}
	double scale = parent_height / total_height;

	// Resize
	wlr_log(L_DEBUG, "Arranging %p vertically", parent);
	double child_y = parent->y + parent_offset;
	for (size_t i = 0; i < num_children; ++i) {
		struct sway_container *child = parent->children->items[i];
		wlr_log(L_DEBUG,
				"Calculating arrangement for %p:%d (will scale %f by %f)",
				child, child->type, child->height, scale);
		child->x = parent->x;
		child->y = child_y;
		child->width = parent->width;
		child->height = floor(child->height * scale);
		child_y += child->height;

		// Make last child use remaining height of parent
		if (i == num_children - 1) {
			child->height =
				parent->y + parent_offset + parent_height - child->y;
		}
		add_gaps(child);
	}
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
		remove_gaps(child);
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent_height;
		add_gaps(child);
	}
}

/**
 * If a container has been deleted from the pending tree state, we must add it
 * to the transaction so it can be freed afterwards. To do this, we iterate the
 * server's destroying_containers list and add all of them. We may add more than
 * what we need to, but this is easy and has no negative consequences.
 */
static void add_deleted_containers(struct sway_transaction *transaction) {
	for (int i = 0; i < server.destroying_containers->length; ++i) {
		struct sway_container *child = server.destroying_containers->items[i];
		transaction_add_container(transaction, child);
	}
}

static void arrange_children_of(struct sway_container *parent,
		struct sway_transaction *transaction);

static void arrange_floating(struct sway_container *floating,
		struct sway_transaction *transaction) {
	for (int i = 0; i < floating->children->length; ++i) {
		struct sway_container *floater = floating->children->items[i];
		if (floater->type == C_VIEW) {
			view_autoconfigure(floater->sway_view);
		} else {
			arrange_children_of(floater, transaction);
		}
		transaction_add_container(transaction, floater);
	}
	transaction_add_container(transaction, floating);
}

static void arrange_children_of(struct sway_container *parent,
		struct sway_transaction *transaction) {
	if (config->reloading) {
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
	case L_NONE:
		apply_horiz_layout(parent);
		break;
	case L_FLOATING:
		arrange_floating(parent, transaction);
		break;
	}

	// Recurse into child containers
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		if (parent->has_gaps && !child->has_gaps) {
			child->has_gaps = true;
			child->gaps_inner = parent->gaps_inner;
			child->gaps_outer = parent->gaps_outer;
		}
		if (child->type == C_VIEW) {
			view_autoconfigure(child->sway_view);
		} else {
			arrange_children_of(child, transaction);
		}
		transaction_add_container(transaction, child);
	}
}

static void arrange_workspace(struct sway_container *workspace,
		struct sway_transaction *transaction) {
	if (config->reloading) {
		return;
	}
	struct sway_container *output = workspace->parent;
	struct wlr_box *area = &output->sway_output->usable_area;
	wlr_log(L_DEBUG, "Usable area for ws: %dx%d@%d,%d",
			area->width, area->height, area->x, area->y);
	remove_gaps(workspace);
	workspace->width = area->width;
	workspace->height = area->height;
	workspace->x = output->x + area->x;
	workspace->y = output->y + area->y;
	add_gaps(workspace);
	transaction_add_container(transaction, workspace);
	wlr_log(L_DEBUG, "Arranging workspace '%s' at %f, %f", workspace->name,
			workspace->x, workspace->y);
	arrange_floating(workspace->sway_workspace->floating, transaction);
	arrange_children_of(workspace, transaction);
}

static void arrange_output(struct sway_container *output,
		struct sway_transaction *transaction) {
	if (config->reloading) {
		return;
	}
	const struct wlr_box *output_box = wlr_output_layout_get_box(
			root_container.sway_root->output_layout,
			output->sway_output->wlr_output);
	output->x = output_box->x;
	output->y = output_box->y;
	output->width = output_box->width;
	output->height = output_box->height;
	transaction_add_container(transaction, output);
	wlr_log(L_DEBUG, "Arranging output '%s' at %f,%f",
			output->name, output->x, output->y);
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		arrange_workspace(workspace, transaction);
	}
}

static void arrange_root(struct sway_transaction *transaction) {
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
	transaction_add_container(transaction, &root_container);
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		arrange_output(output, transaction);
	}
}

void arrange_windows(struct sway_container *container,
		struct sway_transaction *transaction) {
	switch (container->type) {
	case C_ROOT:
		arrange_root(transaction);
		break;
	case C_OUTPUT:
		arrange_output(container, transaction);
		break;
	case C_WORKSPACE:
		arrange_workspace(container, transaction);
		break;
	case C_CONTAINER:
		arrange_children_of(container, transaction);
		transaction_add_container(transaction, container);
		break;
	case C_VIEW:
		view_autoconfigure(container->sway_view);
		transaction_add_container(transaction, container);
		break;
	case C_TYPES:
		break;
	}
	// Add damage for whatever container arrange_windows() was called with,
	// unless it was called with the special floating container, in which case
	// we'll damage the entire output.
	if (container->type == C_CONTAINER && container->layout == L_FLOATING) {
		struct sway_container *output = container_parent(container, C_OUTPUT);
		transaction_add_damage(transaction, container_get_box(output));
	} else {
		transaction_add_damage(transaction, container_get_box(container));
	}
	add_deleted_containers(transaction);
}

void arrange_and_commit(struct sway_container *container) {
	struct sway_transaction *transaction = transaction_create();
	arrange_windows(container, transaction);
	transaction_commit(transaction);
}

void remove_gaps(struct sway_container *c) {
	if (c->current_gaps == 0) {
		wlr_log(L_DEBUG, "Removing gaps: not gapped: %p", c);
		return;
	}

	c->width += c->current_gaps * 2;
	c->height += c->current_gaps * 2;
	c->x -= c->current_gaps;
	c->y -= c->current_gaps;

	c->current_gaps = 0;

	wlr_log(L_DEBUG, "Removing gaps %p", c);
}

void add_gaps(struct sway_container *c) {
	if (c->current_gaps > 0 || c->type == C_CONTAINER) {
		wlr_log(L_DEBUG, "Not adding gaps: %p", c);
		return;
	}

	if (c->type == C_WORKSPACE &&
		!(config->edge_gaps || (config->smart_gaps && c->children->length > 1))) {
		return;
	}

	double gaps = c->has_gaps ? c->gaps_inner : config->gaps_inner;

	c->x += gaps;
	c->y += gaps;
	c->width -= 2 * gaps;
	c->height -= 2 * gaps;
	c->current_gaps = gaps;

	wlr_log(L_DEBUG, "Adding gaps: %p", c);
}
