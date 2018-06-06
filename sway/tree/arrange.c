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
		child->x = parent->x;
		child->y = parent->y + parent_offset;
		child->width = parent->width;
		child->height = parent_height;
	}
}

static void _arrange_children_of(struct sway_container *parent,
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
		sway_assert(false, "Didn't expect to see floating here");
	}

	// Recurse into child containers
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		if (child->type == C_VIEW) {
			view_autoconfigure(child->sway_view);
		} else {
			_arrange_children_of(child, transaction);
		}
		transaction_add_container(transaction, child);
	}
}

static void _arrange_workspace(struct sway_container *workspace,
		struct sway_transaction *transaction) {
	if (config->reloading) {
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
	transaction_add_container(transaction, workspace);
	wlr_log(L_DEBUG, "Arranging workspace '%s' at %f, %f", workspace->name,
			workspace->x, workspace->y);
	_arrange_children_of(workspace, transaction);
}

static void _arrange_output(struct sway_container *output,
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
		_arrange_workspace(workspace, transaction);
	}
}

static void _arrange_root(struct sway_transaction *transaction) {
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
		_arrange_output(output, transaction);
	}
}

void arrange_windows(struct sway_container *container,
		struct sway_transaction *transaction) {
	switch (container->type) {
	case C_ROOT:
		_arrange_root(transaction);
		break;
	case C_OUTPUT:
		_arrange_output(container, transaction);
		break;
	case C_WORKSPACE:
		_arrange_workspace(container, transaction);
		break;
	case C_CONTAINER:
		_arrange_children_of(container, transaction);
		transaction_add_container(transaction, container);
		break;
	case C_VIEW:
		break;
	case C_TYPES:
		break;
	}
	transaction_add_damage(transaction, container_get_box(container));
}

void arrange_and_commit(struct sway_container *container) {
	struct sway_transaction *transaction = transaction_create();
	arrange_windows(container, transaction);
	transaction_commit(transaction);
}

// These functions are only temporary
void arrange_root() {
	arrange_and_commit(&root_container);
}

void arrange_output(struct sway_container *container) {
	arrange_and_commit(container);
}

void arrange_workspace(struct sway_container *container) {
	arrange_and_commit(container);
}

void arrange_children_of(struct sway_container *container) {
	arrange_and_commit(container);
}
