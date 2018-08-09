#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "log.h"

static const int MIN_SANE_W = 100, MIN_SANE_H = 60;

enum resize_unit {
	RESIZE_UNIT_PX,
	RESIZE_UNIT_PPT,
	RESIZE_UNIT_DEFAULT,
	RESIZE_UNIT_INVALID,
};

enum resize_axis {
	RESIZE_AXIS_HORIZONTAL,
	RESIZE_AXIS_VERTICAL,
	RESIZE_AXIS_UP,
	RESIZE_AXIS_DOWN,
	RESIZE_AXIS_LEFT,
	RESIZE_AXIS_RIGHT,
	RESIZE_AXIS_INVALID,
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
	struct sway_container *con = config->handler_context.current_container;

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

	if (config->floating_maximum_width == -1) { // no maximum
		*max_width = INT_MAX;
	} else if (config->floating_maximum_width == 0) { // automatic
		struct sway_container *ws = container_parent(con, C_WORKSPACE);
		*max_width = ws->width;
	} else {
		*max_width = config->floating_maximum_width;
	}

	if (config->floating_maximum_height == -1) { // no maximum
		*max_height = INT_MAX;
	} else if (config->floating_maximum_height == 0) { // automatic
		struct sway_container *ws = container_parent(con, C_WORKSPACE);
		*max_height = ws->height;
	} else {
		*max_height = config->floating_maximum_height;
	}
}

static enum resize_axis parse_resize_axis(const char *axis) {
	if (strcasecmp(axis, "width") == 0 || strcasecmp(axis, "horizontal") == 0) {
		return RESIZE_AXIS_HORIZONTAL;
	}
	if (strcasecmp(axis, "height") == 0 || strcasecmp(axis, "vertical") == 0) {
		return RESIZE_AXIS_VERTICAL;
	}
	if (strcasecmp(axis, "up") == 0) {
		return RESIZE_AXIS_UP;
	}
	if (strcasecmp(axis, "down") == 0) {
		return RESIZE_AXIS_DOWN;
	}
	if (strcasecmp(axis, "left") == 0) {
		return RESIZE_AXIS_LEFT;
	}
	if (strcasecmp(axis, "right") == 0) {
		return RESIZE_AXIS_RIGHT;
	}
	return RESIZE_AXIS_INVALID;
}

static enum resize_axis normalize_axis(enum resize_axis axis) {
	switch (axis) {
	case RESIZE_AXIS_HORIZONTAL:
	case RESIZE_AXIS_LEFT:
	case RESIZE_AXIS_RIGHT:
		return RESIZE_AXIS_HORIZONTAL;
	case RESIZE_AXIS_VERTICAL:
	case RESIZE_AXIS_UP:
	case RESIZE_AXIS_DOWN:
		return RESIZE_AXIS_VERTICAL;
	case RESIZE_AXIS_INVALID:
		sway_assert(false, "Never reached");
	}
	sway_assert(false, "Never reached");
	return RESIZE_AXIS_INVALID;
}

static int parallel_coord(struct sway_container *c, enum resize_axis a) {
	return normalize_axis(a) == RESIZE_AXIS_HORIZONTAL ? c->x : c->y;
}

static int parallel_size(struct sway_container *c, enum resize_axis a) {
	return normalize_axis(a) == RESIZE_AXIS_HORIZONTAL ? c->width : c->height;
}

static void resize_tiled(int amount, enum resize_axis axis) {
	struct sway_container *parent = config->handler_context.current_container;
	struct sway_container *focused = parent;
	if (!parent) {
		return;
	}

	enum sway_container_layout parallel_layout =
		normalize_axis(axis) == RESIZE_AXIS_HORIZONTAL ? L_HORIZ : L_VERT;
	int minor_weight = 0;
	int major_weight = 0;
	while (parent->parent) {
		struct sway_container *next = parent->parent;
		if (next->layout == parallel_layout) {
			for (int i = 0; i < next->children->length; i++) {
				struct sway_container *sibling = next->children->items[i];

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
		parent = next;
	}

	if (parent->type == C_ROOT) {
		return;
	}

	wlr_log(WLR_DEBUG,
			"Found the proper parent: %p. It has %d l conts, and %d r conts",
			parent->parent, minor_weight, major_weight);

	// Implement up/down/left/right direction by zeroing one of the weights,
	// then setting the axis to be horizontal or vertical
	if (axis == RESIZE_AXIS_UP || axis == RESIZE_AXIS_LEFT) {
		major_weight = 0;
	} else if (axis == RESIZE_AXIS_RIGHT || axis == RESIZE_AXIS_DOWN) {
		minor_weight = 0;
	}
	axis = normalize_axis(axis);

	int min_sane = axis == RESIZE_AXIS_HORIZONTAL ? MIN_SANE_W : MIN_SANE_H;

	//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
	// ^ ?????

	for (int i = 0; i < parent->parent->children->length; i++) {
		struct sway_container *sibling = parent->parent->children->items[i];

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

	enum resize_edge minor_edge = axis == RESIZE_AXIS_HORIZONTAL ?
		RESIZE_EDGE_LEFT : RESIZE_EDGE_TOP;
	enum resize_edge major_edge = axis == RESIZE_AXIS_HORIZONTAL ?
		RESIZE_EDGE_RIGHT : RESIZE_EDGE_BOTTOM;

	for (int i = 0; i < parent->parent->children->length; i++) {
		struct sway_container *sibling = parent->parent->children->items[i];

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

	arrange_windows(parent->parent);
}

/**
 * Implement `resize <grow|shrink>` for a floating container.
 */
static struct cmd_results *resize_adjust_floating(enum resize_axis axis,
		struct resize_amount *amount) {
	struct sway_container *con = config->handler_context.current_container;
	int grow_width = 0, grow_height = 0;
	switch (axis) {
	case RESIZE_AXIS_HORIZONTAL:
	case RESIZE_AXIS_LEFT:
	case RESIZE_AXIS_RIGHT:
		grow_width = amount->amount;
		break;
	case RESIZE_AXIS_VERTICAL:
	case RESIZE_AXIS_UP:
	case RESIZE_AXIS_DOWN:
		grow_height = amount->amount;
		break;
	case RESIZE_AXIS_INVALID:
		return cmd_results_new(CMD_INVALID, "resize", "Invalid axis/direction");
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
	switch (axis) {
	case RESIZE_AXIS_HORIZONTAL:
		grow_x = -grow_width / 2;
		break;
	case RESIZE_AXIS_VERTICAL:
		grow_y = -grow_height / 2;
		break;
	case RESIZE_AXIS_UP:
		grow_y = -grow_height;
		break;
	case RESIZE_AXIS_LEFT:
		grow_x = -grow_width;
		break;
	case RESIZE_AXIS_DOWN:
	case RESIZE_AXIS_RIGHT:
		break;
	case RESIZE_AXIS_INVALID:
		return cmd_results_new(CMD_INVALID, "resize", "Invalid axis/direction");
	}
	con->x += grow_x;
	con->y += grow_y;
	con->width += grow_width;
	con->height += grow_height;

	if (con->type == C_VIEW) {
		struct sway_view *view = con->sway_view;
		view->x += grow_x;
		view->y += grow_y;
		view->width += grow_width;
		view->height += grow_height;
	}

	arrange_windows(con);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

/**
 * Implement `resize <grow|shrink>` for a tiled container.
 */
static struct cmd_results *resize_adjust_tiled(enum resize_axis axis,
		struct resize_amount *amount) {
	struct sway_container *current = config->handler_context.current_container;

	if (amount->unit == RESIZE_UNIT_DEFAULT) {
		amount->unit = RESIZE_UNIT_PPT;
	}
	if (amount->unit == RESIZE_UNIT_PPT) {
		float pct = amount->amount / 100.0f;
		switch (axis) {
		case RESIZE_AXIS_LEFT:
		case RESIZE_AXIS_RIGHT:
		case RESIZE_AXIS_HORIZONTAL:
			amount->amount = (float)current->width * pct;
			break;
		case RESIZE_AXIS_UP:
		case RESIZE_AXIS_DOWN:
		case RESIZE_AXIS_VERTICAL:
			amount->amount = (float)current->height * pct;
			break;
		case RESIZE_AXIS_INVALID:
			return cmd_results_new(CMD_INVALID, "resize",
					"Invalid resize axis/direction");
		}
	}

	resize_tiled(amount->amount, axis);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
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
			while (parent->type >= C_WORKSPACE && parent->layout != L_HORIZ) {
				parent = parent->parent;
			}
			if (parent->type >= C_WORKSPACE) {
				width->amount = parent->width * width->amount / 100;
				width->unit = RESIZE_UNIT_PX;
			}
		}
		if (width->unit == RESIZE_UNIT_PX) {
			resize_tiled(width->amount - con->width, RESIZE_AXIS_HORIZONTAL);
		}
	}

	if (height->amount) {
		if (height->unit == RESIZE_UNIT_PPT ||
				height->unit == RESIZE_UNIT_DEFAULT) {
			// Convert to px
			struct sway_container *parent = con->parent;
			while (parent->type >= C_WORKSPACE && parent->layout != L_VERT) {
				parent = parent->parent;
			}
			if (parent->type >= C_WORKSPACE) {
				height->amount = parent->height * height->amount / 100;
				height->unit = RESIZE_UNIT_PX;
			}
		}
		if (height->unit == RESIZE_UNIT_PX) {
			resize_tiled(height->amount - con->height, RESIZE_AXIS_VERTICAL);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

/**
 * Implement `resize set` for a floating container.
 */
static struct cmd_results *resize_set_floating(struct sway_container *con,
		struct resize_amount *width, struct resize_amount *height) {
	int min_width, max_width, min_height, max_height;
	calculate_constraints(&min_width, &max_width, &min_height, &max_height);
	width->amount = fmax(min_width, fmin(width->amount, max_width));
	height->amount = fmax(min_height, fmin(height->amount, max_height));
	int grow_width = width->amount - con->width;
	int grow_height = height->amount - con->height;
	con->x -= grow_width / 2;
	con->y -= grow_height / 2;
	con->width = width->amount;
	con->height = height->amount;

	if (con->type == C_VIEW) {
		struct sway_view *view = con->sway_view;
		view->x -= grow_width / 2;
		view->y -= grow_height / 2;
		view->width += grow_width;
		view->height += grow_height;
	}

	arrange_windows(con);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

/**
 * resize set <args>
 *
 * args: <width> [px|ppt] <height> [px|ppt]
 */
static struct cmd_results *cmd_resize_set(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 2))) {
		return error;
	}
	const char *usage = "Expected 'resize set <width> <height>'";

	// Width
	struct resize_amount width;
	int num_consumed_args = parse_resize_amount(argc, argv, &width);
	argc -= num_consumed_args;
	argv += num_consumed_args;
	if (width.unit == RESIZE_UNIT_INVALID) {
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}

	// Height
	struct resize_amount height;
	num_consumed_args = parse_resize_amount(argc, argv, &height);
	argc -= num_consumed_args;
	argv += num_consumed_args;
	if (height.unit == RESIZE_UNIT_INVALID) {
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}

	// If 0, don't resize that dimension
	struct sway_container *con = config->handler_context.current_container;
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
	const char *usage = "Expected 'resize grow|shrink <direction> "
		"[<amount> px|ppt [or <amount> px|ppt]]'";
	enum resize_axis axis = parse_resize_axis(*argv);
	if (axis == RESIZE_AXIS_INVALID) {
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}
	--argc; ++argv;

	// First amount
	struct resize_amount first_amount;
	if (argc) {
		int num_consumed_args = parse_resize_amount(argc, argv, &first_amount);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (first_amount.unit == RESIZE_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, "resize", usage);
		}
	} else {
		first_amount.amount = 10;
		first_amount.unit = RESIZE_UNIT_DEFAULT;
	}

	// "or"
	if (argc) {
		if (strcmp(*argv, "or") != 0) {
			return cmd_results_new(CMD_INVALID, "resize", usage);
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
			return cmd_results_new(CMD_INVALID, "resize", usage);
		}
	} else {
		second_amount.unit = RESIZE_UNIT_INVALID;
	}

	first_amount.amount *= multiplier;
	second_amount.amount *= multiplier;

	struct sway_container *con = config->handler_context.current_container;
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
			return cmd_results_new(CMD_INVALID, "resize",
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
	struct sway_container *current = config->handler_context.current_container;
	if (!current) {
		return cmd_results_new(CMD_INVALID, "resize", "Cannot resize nothing");
	}
	if (current->type != C_VIEW && current->type != C_CONTAINER) {
		return cmd_results_new(CMD_INVALID, "resize",
				"Can only resize views/containers");
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

	const char *usage = "Expected 'resize <shrink|grow> "
		"<width|height|up|down|left|right> [<amount>] [px|ppt]'";

	return cmd_results_new(CMD_INVALID, "resize", usage);
}
