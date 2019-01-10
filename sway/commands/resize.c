#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "log.h"

#define AXIS_HORIZONTAL (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)
#define AXIS_VERTICAL   (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)

static const int MIN_SANE_W = 100, MIN_SANE_H = 60;

enum resize_unit {
	RESIZE_UNIT_PX,
	RESIZE_UNIT_PPT,
	RESIZE_UNIT_DEFAULT,
	RESIZE_UNIT_INVALID,
};

struct resize_amount {
	int amount;
	enum resize_unit unit;
};

static enum resize_unit parse_resize_unit(const char *unit) {
	if (strcasecmp(unit, "px") == 0) {
		return RESIZE_UNIT_PX;
	}
	if (strcasecmp(unit, "ppt") == 0) {
		return RESIZE_UNIT_PPT;
	}
	if (strcasecmp(unit, "default") == 0) {
		return RESIZE_UNIT_DEFAULT;
	}
	return RESIZE_UNIT_INVALID;
}

// Parse arguments such as "10", "10px" or "10 px".
// Returns the number of arguments consumed.
static int parse_resize_amount(int argc, char **argv,
		struct resize_amount *amount) {
	char *err;
	amount->amount = (int)strtol(argv[0], &err, 10);
	if (*err) {
		// e.g. 10px
		amount->unit = parse_resize_unit(err);
		return 1;
	}
	if (argc == 1) {
		amount->unit = RESIZE_UNIT_DEFAULT;
		return 1;
	}
	// Try the second argument
	amount->unit = parse_resize_unit(argv[1]);
	if (amount->unit == RESIZE_UNIT_INVALID) {
		amount->unit = RESIZE_UNIT_DEFAULT;
		return 1;
	}
	return 2;
}

static void calculate_constraints(int *min_width, int *max_width,
		int *min_height, int *max_height) {
	struct sway_container *con = config->handler_context.container;

	if (config->floating_minimum_width == -1) { // no minimum
		*min_width = 0;
	} else if (config->floating_minimum_width == 0) { // automatic
		*min_width = 75;
	} else {
		*min_width = config->floating_minimum_width;
	}

	if (config->floating_minimum_height == -1) { // no minimum
		*min_height = 0;
	} else if (config->floating_minimum_height == 0) { // automatic
		*min_height = 50;
	} else {
		*min_height = config->floating_minimum_height;
	}

	if (config->floating_maximum_width == -1 || !con->workspace) { // no max
		*max_width = INT_MAX;
	} else if (config->floating_maximum_width == 0) { // automatic
		*max_width = con->workspace->width;
	} else {
		*max_width = config->floating_maximum_width;
	}

	if (config->floating_maximum_height == -1 || !con->workspace) { // no max
		*max_height = INT_MAX;
	} else if (config->floating_maximum_height == 0) { // automatic
		*max_height = con->workspace->height;
	} else {
		*max_height = config->floating_maximum_height;
	}
}

static uint32_t parse_resize_axis(const char *axis) {
	if (strcasecmp(axis, "width") == 0 || strcasecmp(axis, "horizontal") == 0) {
		return AXIS_HORIZONTAL;
	}
	if (strcasecmp(axis, "height") == 0 || strcasecmp(axis, "vertical") == 0) {
		return AXIS_VERTICAL;
	}
	if (strcasecmp(axis, "up") == 0) {
		return WLR_EDGE_TOP;
	}
	if (strcasecmp(axis, "down") == 0) {
		return WLR_EDGE_BOTTOM;
	}
	if (strcasecmp(axis, "left") == 0) {
		return WLR_EDGE_LEFT;
	}
	if (strcasecmp(axis, "right") == 0) {
		return WLR_EDGE_RIGHT;
	}
	return WLR_EDGE_NONE;
}

static bool is_horizontal(uint32_t axis) {
	return axis & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
}

static int parallel_coord(struct sway_container *c, uint32_t axis) {
	return is_horizontal(axis) ? c->x : c->y;
}

static int parallel_size(struct sway_container *c, uint32_t axis) {
	return is_horizontal(axis) ? c->width : c->height;
}

static void container_recursive_resize(struct sway_container *container,
		double amount, enum wlr_edges edge) {
	bool layout_match = true;
	wlr_log(WLR_DEBUG, "Resizing %p with amount: %f", container, amount);
	if (edge & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
		container->width += amount;
		layout_match = container->layout == L_HORIZ;
	} else if (edge & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) {
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

static void resize_tiled(struct sway_container *parent, int amount,
		uint32_t axis) {
	struct sway_container *focused = parent;
	if (!parent) {
		return;
	}

	enum sway_container_layout parallel_layout =
		is_horizontal(axis) ? L_HORIZ : L_VERT;
	int minor_weight = 0;
	int major_weight = 0;
	while (parent) {
		list_t *siblings = container_get_siblings(parent);
		if (container_parent_layout(parent) == parallel_layout) {
			for (int i = 0; i < siblings->length; i++) {
				struct sway_container *sibling = siblings->items[i];

				int sibling_pos = parallel_coord(sibling, axis);
				int focused_pos = parallel_coord(focused, axis);
				int parent_pos = parallel_coord(parent, axis);

				if (sibling_pos != focused_pos) {
					if (sibling_pos < parent_pos) {
						minor_weight++;
					} else if (sibling_pos > parent_pos) {
						major_weight++;
					}
				}
			}
			if (major_weight || minor_weight) {
				break;
			}
		}
		parent = parent->parent;
	}
	if (!parent) {
		// Can't resize in this direction
		return;
	}

	// Implement up/down/left/right direction by zeroing one of the weights
	if (axis == WLR_EDGE_TOP || axis == WLR_EDGE_LEFT) {
		major_weight = 0;
	} else if (axis == WLR_EDGE_RIGHT || axis == WLR_EDGE_BOTTOM) {
		minor_weight = 0;
	}

	bool horizontal = is_horizontal(axis);
	int min_sane = horizontal ? MIN_SANE_W : MIN_SANE_H;

	//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
	// ^ ?????
	list_t *siblings = container_get_siblings(parent);

	for (int i = 0; i < siblings->length; i++) {
		struct sway_container *sibling = siblings->items[i];

		int sibling_pos = parallel_coord(sibling, axis);
		int focused_pos = parallel_coord(focused, axis);
		int parent_pos = parallel_coord(parent, axis);

		int sibling_size = parallel_size(sibling, axis);
		int parent_size = parallel_size(parent, axis);

		if (sibling_pos != focused_pos) {
			if (sibling_pos < parent_pos && minor_weight) {
				double pixels = -amount / minor_weight;
				if (major_weight && (sibling_size + pixels / 2) < min_sane) {
					return; // Too small
				} else if (!major_weight && sibling_size + pixels < min_sane) {
					return; // Too small
				}
			} else if (sibling_pos > parent_pos && major_weight) {
				double pixels = -amount / major_weight;
				if (minor_weight && (sibling_size + pixels / 2) < min_sane) {
					return; // Too small
				} else if (!minor_weight && sibling_size + pixels < min_sane) {
					return; // Too small
				}
			}
		} else {
			double pixels = amount;
			if (parent_size + pixels < min_sane) {
				return; // Too small
			}
		}
	}

	enum wlr_edges minor_edge = horizontal ? WLR_EDGE_LEFT : WLR_EDGE_TOP;
	enum wlr_edges major_edge = horizontal ? WLR_EDGE_RIGHT : WLR_EDGE_BOTTOM;

	for (int i = 0; i < siblings->length; i++) {
		struct sway_container *sibling = siblings->items[i];

		int sibling_pos = parallel_coord(sibling, axis);
		int focused_pos = parallel_coord(focused, axis);
		int parent_pos = parallel_coord(parent, axis);

		if (sibling_pos != focused_pos) {
			if (sibling_pos < parent_pos && minor_weight) {
				double pixels = -1 * amount;
				pixels /= minor_weight;
				if (major_weight) {
					container_recursive_resize(sibling, pixels / 2, major_edge);
				} else {
					container_recursive_resize(sibling, pixels, major_edge);
				}
			} else if (sibling_pos > parent_pos && major_weight) {
				double pixels = -1 * amount;
				pixels /= major_weight;
				if (minor_weight) {
					container_recursive_resize(sibling, pixels / 2, minor_edge);
				} else {
					container_recursive_resize(sibling, pixels, minor_edge);
				}
			}
		} else {
			if (major_weight != 0 && minor_weight != 0) {
				double pixels = amount;
				pixels /= 2;
				container_recursive_resize(parent, pixels, minor_edge);
				container_recursive_resize(parent, pixels, major_edge);
			} else if (major_weight) {
				container_recursive_resize(parent, amount, major_edge);
			} else if (minor_weight) {
				container_recursive_resize(parent, amount, minor_edge);
			}
		}
	}

	if (parent->parent) {
		arrange_container(parent->parent);
	} else {
		arrange_workspace(parent->workspace);
	}
}

void container_resize_tiled(struct sway_container *parent,
		enum wlr_edges edge, int amount) {
	resize_tiled(parent, amount, edge);
}

/**
 * Implement `resize <grow|shrink>` for a floating container.
 */
static struct cmd_results *resize_adjust_floating(uint32_t axis,
		struct resize_amount *amount) {
	struct sway_container *con = config->handler_context.container;
	int grow_width = 0, grow_height = 0;

	if (is_horizontal(axis)) {
		grow_width = amount->amount;
	} else {
		grow_height = amount->amount;
	}

	// Make sure we're not adjusting beyond floating min/max size
	int min_width, max_width, min_height, max_height;
	calculate_constraints(&min_width, &max_width, &min_height, &max_height);
	if (con->width + grow_width < min_width) {
		grow_width = min_width - con->width;
	} else if (con->width + grow_width > max_width) {
		grow_width = max_width - con->width;
	}
	if (con->height + grow_height < min_height) {
		grow_height = min_height - con->height;
	} else if (con->height + grow_height > max_height) {
		grow_height = max_height - con->height;
	}
	int grow_x = 0, grow_y = 0;

	if (axis == AXIS_HORIZONTAL) {
		grow_x = -grow_width / 2;
	} else if (axis == AXIS_VERTICAL) {
		grow_y = -grow_height / 2;
	} else if (axis == WLR_EDGE_TOP) {
		grow_y = -grow_height;
	} else if (axis == WLR_EDGE_LEFT) {
		grow_x = -grow_width;
	}
	if (grow_x == 0 && grow_y == 0) {
		return cmd_results_new(CMD_INVALID, "Cannot resize any further");
	}
	con->x += grow_x;
	con->y += grow_y;
	con->width += grow_width;
	con->height += grow_height;

	con->content_x += grow_x;
	con->content_y += grow_y;
	con->content_width += grow_width;
	con->content_height += grow_height;

	arrange_container(con);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Implement `resize <grow|shrink>` for a tiled container.
 */
static struct cmd_results *resize_adjust_tiled(uint32_t axis,
		struct resize_amount *amount) {
	struct sway_container *current = config->handler_context.container;

	if (amount->unit == RESIZE_UNIT_DEFAULT) {
		amount->unit = RESIZE_UNIT_PPT;
	}
	if (amount->unit == RESIZE_UNIT_PPT) {
		float pct = amount->amount / 100.0f;

		if (is_horizontal(axis)) {
			amount->amount = (float)current->width * pct;
		} else {
			amount->amount = (float)current->height * pct;
		}
	}

	double old_width = current->width;
	double old_height = current->height;
	resize_tiled(current, amount->amount, axis);
	if (current->width == old_width && current->height == old_height) {
		return cmd_results_new(CMD_INVALID, "Cannot resize any further");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Implement `resize set` for a tiled container.
 */
static struct cmd_results *resize_set_tiled(struct sway_container *con,
		struct resize_amount *width, struct resize_amount *height) {
	if (width->amount) {
		if (width->unit == RESIZE_UNIT_PPT ||
				width->unit == RESIZE_UNIT_DEFAULT) {
			// Convert to px
			struct sway_container *parent = con->parent;
			while (parent && parent->layout != L_HORIZ) {
				parent = parent->parent;
			}
			if (parent) {
				width->amount = parent->width * width->amount / 100;
			} else {
				width->amount = con->workspace->width * width->amount / 100;
			}
			width->unit = RESIZE_UNIT_PX;
		}
		if (width->unit == RESIZE_UNIT_PX) {
			resize_tiled(con, width->amount - con->width, AXIS_HORIZONTAL);
		}
	}

	if (height->amount) {
		if (height->unit == RESIZE_UNIT_PPT ||
				height->unit == RESIZE_UNIT_DEFAULT) {
			// Convert to px
			struct sway_container *parent = con->parent;
			while (parent && parent->layout != L_VERT) {
				parent = parent->parent;
			}
			if (parent) {
				height->amount = parent->height * height->amount / 100;
			} else {
				height->amount = con->workspace->height * height->amount / 100;
			}
			height->unit = RESIZE_UNIT_PX;
		}
		if (height->unit == RESIZE_UNIT_PX) {
			resize_tiled(con, height->amount - con->height, AXIS_VERTICAL);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Implement `resize set` for a floating container.
 */
static struct cmd_results *resize_set_floating(struct sway_container *con,
		struct resize_amount *width, struct resize_amount *height) {
	int min_width, max_width, min_height, max_height, grow_width = 0, grow_height = 0;
	calculate_constraints(&min_width, &max_width, &min_height, &max_height);

	if (width->amount) {
		switch (width->unit) {
		case RESIZE_UNIT_PPT:
			// Convert to px
			width->amount = con->workspace->width * width->amount / 100;
			width->unit = RESIZE_UNIT_PX;
			// Falls through
		case RESIZE_UNIT_PX:
		case RESIZE_UNIT_DEFAULT:
			width->amount = fmax(min_width, fmin(width->amount, max_width));
			grow_width = width->amount - con->width;
			con->x -= grow_width / 2;
			con->width = width->amount;
			break;
		case RESIZE_UNIT_INVALID:
			sway_assert(false, "invalid width unit");
			break;
		}
	}

	if (height->amount) {
		switch (height->unit) {
		case RESIZE_UNIT_PPT:
			// Convert to px
			height->amount = con->workspace->height * height->amount / 100;
			height->unit = RESIZE_UNIT_PX;
			// Falls through
		case RESIZE_UNIT_PX:
		case RESIZE_UNIT_DEFAULT:
			height->amount = fmax(min_height, fmin(height->amount, max_height));
			grow_height = height->amount - con->height;
			con->y -= grow_height / 2;
			con->height = height->amount;
			break;
		case RESIZE_UNIT_INVALID:
			sway_assert(false, "invalid height unit");
			break;
		}
	}

	con->content_x -= grow_width / 2;
	con->content_y -= grow_height / 2;
	con->content_width += grow_width;
	con->content_height += grow_height;

	arrange_container(con);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * resize set <args>
 *
 * args: [width] <width> [px|ppt]
 *     : height <height> [px|ppt]
 *     : [width] <width> [px|ppt] [height] <height> [px|ppt]
 */
static struct cmd_results *cmd_resize_set(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char usage[] = "Expected 'resize set [width] <width> [px|ppt]' or "
		"'resize set height <height> [px|ppt]' or "
		"'resize set [width] <width> [px|ppt] [height] <height> [px|ppt]'";

	// Width
	struct resize_amount width = {0};
	if (argc >= 2 && !strcmp(argv[0], "width") && strcmp(argv[1], "height")) {
		argc--; argv++;
	}
	if (strcmp(argv[0], "height")) {
		int num_consumed_args = parse_resize_amount(argc, argv, &width);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (width.unit == RESIZE_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, usage);
		}
	}

	// Height
	struct resize_amount height = {0};
	if (argc) {
		if (argc >= 2 && !strcmp(argv[0], "height")) {
			argc--; argv++;
		}
		int num_consumed_args = parse_resize_amount(argc, argv, &height);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (width.unit == RESIZE_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, usage);
		}
	}

	// If 0, don't resize that dimension
	struct sway_container *con = config->handler_context.container;
	if (width.amount <= 0) {
		width.amount = con->width;
	}
	if (height.amount <= 0) {
		height.amount = con->height;
	}

	if (container_is_floating(con)) {
		return resize_set_floating(con, &width, &height);
	}
	return resize_set_tiled(con, &width, &height);
}

/**
 * resize <grow|shrink> <args>
 *
 * args: <direction>
 * args: <direction> <amount> <unit>
 * args: <direction> <amount> <unit> or <amount> <other_unit>
 */
static struct cmd_results *cmd_resize_adjust(int argc, char **argv,
		int multiplier) {
	const char usage[] = "Expected 'resize grow|shrink <direction> "
		"[<amount> px|ppt [or <amount> px|ppt]]'";
	uint32_t axis = parse_resize_axis(*argv);
	if (axis == WLR_EDGE_NONE) {
		return cmd_results_new(CMD_INVALID, usage);
	}
	--argc; ++argv;

	// First amount
	struct resize_amount first_amount;
	if (argc) {
		int num_consumed_args = parse_resize_amount(argc, argv, &first_amount);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (first_amount.unit == RESIZE_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, usage);
		}
	} else {
		first_amount.amount = 10;
		first_amount.unit = RESIZE_UNIT_DEFAULT;
	}

	// "or"
	if (argc) {
		if (strcmp(*argv, "or") != 0) {
			return cmd_results_new(CMD_INVALID, usage);
		}
		--argc; ++argv;
	}

	// Second amount
	struct resize_amount second_amount;
	if (argc) {
		int num_consumed_args = parse_resize_amount(argc, argv, &second_amount);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (second_amount.unit == RESIZE_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, usage);
		}
	} else {
		second_amount.unit = RESIZE_UNIT_INVALID;
	}

	first_amount.amount *= multiplier;
	second_amount.amount *= multiplier;

	struct sway_container *con = config->handler_context.container;
	if (container_is_floating(con)) {
		// Floating containers can only resize in px. Choose an amount which
		// uses px, with fallback to an amount that specified no unit.
		if (first_amount.unit == RESIZE_UNIT_PX) {
			return resize_adjust_floating(axis, &first_amount);
		} else if (second_amount.unit == RESIZE_UNIT_PX) {
			return resize_adjust_floating(axis, &second_amount);
		} else if (first_amount.unit == RESIZE_UNIT_DEFAULT) {
			return resize_adjust_floating(axis, &first_amount);
		} else if (second_amount.unit == RESIZE_UNIT_DEFAULT) {
			return resize_adjust_floating(axis, &second_amount);
		} else {
			return cmd_results_new(CMD_INVALID,
					"Floating containers cannot use ppt measurements");
		}
	}

	// For tiling, prefer ppt -> default -> px
	if (first_amount.unit == RESIZE_UNIT_PPT) {
		return resize_adjust_tiled(axis, &first_amount);
	} else if (second_amount.unit == RESIZE_UNIT_PPT) {
		return resize_adjust_tiled(axis, &second_amount);
	} else if (first_amount.unit == RESIZE_UNIT_DEFAULT) {
		return resize_adjust_tiled(axis, &first_amount);
	} else if (second_amount.unit == RESIZE_UNIT_DEFAULT) {
		return resize_adjust_tiled(axis, &second_amount);
	} else {
		return resize_adjust_tiled(axis, &first_amount);
	}
}

struct cmd_results *cmd_resize(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_container *current = config->handler_context.container;
	if (!current) {
		return cmd_results_new(CMD_INVALID, "Cannot resize nothing");
	}

	struct cmd_results *error;
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (strcasecmp(argv[0], "set") == 0) {
		return cmd_resize_set(argc - 1, &argv[1]);
	}
	if (strcasecmp(argv[0], "grow") == 0) {
		return cmd_resize_adjust(argc - 1, &argv[1], 1);
	}
	if (strcasecmp(argv[0], "shrink") == 0) {
		return cmd_resize_adjust(argc - 1, &argv[1], -1);
	}

	const char usage[] = "Expected 'resize <shrink|grow> "
		"<width|height|up|down|left|right> [<amount>] [px|ppt]'";

	return cmd_results_new(CMD_INVALID, usage);
}
