#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/tree/arrange.h"
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
	RESIZE_AXIS_INVALID,
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

static enum resize_axis parse_resize_axis(const char *axis) {
	if (strcasecmp(axis, "width") == 0 || strcasecmp(axis, "horizontal") == 0) {
		return RESIZE_AXIS_HORIZONTAL;
	}
	if (strcasecmp(axis, "height") == 0 || strcasecmp(axis, "vertical") == 0) {
		return RESIZE_AXIS_VERTICAL;
	}
	return RESIZE_AXIS_INVALID;
}

static int parallel_coord(struct sway_container *c, enum resize_axis a) {
	return a == RESIZE_AXIS_HORIZONTAL ? c->x : c->y;
}

static int parallel_size(struct sway_container *c, enum resize_axis a) {
	return a == RESIZE_AXIS_HORIZONTAL ? c->width : c->height;
}

static void resize_tiled(int amount, enum resize_axis axis) {
	struct sway_container *parent = config->handler_context.current_container;
	struct sway_container *focused = parent;
	if (!parent) {
		return;
	}

	enum sway_container_layout parallel_layout =
		axis == RESIZE_AXIS_HORIZONTAL ? L_HORIZ : L_VERT;
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

	wlr_log(L_DEBUG,
			"Found the proper parent: %p. It has %d l conts, and %d r conts",
			parent->parent, minor_weight, major_weight);

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
			if (sibling_pos < parent_pos) {
				double pixels = -amount / minor_weight;
				if (major_weight && (sibling_size + pixels / 2) < min_sane) {
					return; // Too small
				} else if ((sibling_size + pixels) < min_sane) {
					return; // Too small
				}
			} else if (sibling_pos > parent_pos) {
				double pixels = -amount / major_weight;
				if (minor_weight && (sibling_size + pixels / 2) < min_sane) {
					return; // Too small
				} else if ((sibling_size + pixels) < min_sane) {
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
			if (sibling_pos < parent_pos) {
				double pixels = -1 * amount;
				pixels /= minor_weight;
				if (major_weight) {
					container_recursive_resize(sibling, pixels / 2, major_edge);
				} else {
					container_recursive_resize(sibling, pixels, major_edge);
				}
			} else if (sibling_pos > parent_pos) {
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

	arrange_children_of(parent->parent);
}

static void resize(int amount, enum resize_axis axis, enum resize_unit unit) {
	struct sway_container *current = config->handler_context.current_container;
	if (unit == RESIZE_UNIT_DEFAULT) {
		// Default for tiling; TODO floating should be px
		unit = RESIZE_UNIT_PPT;
	}

	if (unit == RESIZE_UNIT_PPT) {
		float pct = amount / 100.0f;
		switch (axis) {
		case RESIZE_AXIS_HORIZONTAL:
			amount = (float)current->width * pct;
			break;
		case RESIZE_AXIS_VERTICAL:
			amount = (float)current->height * pct;
			break;
		default:
			sway_assert(0, "invalid resize axis");
			return;
		}
	}

	return resize_tiled(amount, axis);
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
	if (strcasecmp(argv[0], "set") == 0) {
		// TODO
		//return cmd_resize_set(argc - 1, &argv[1]);
		return cmd_results_new(CMD_INVALID, "resize", "resize set unimplemented");
	}
	struct cmd_results *error;
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	// TODO: resize grow|shrink left|right|up|down

	const char *usage = "Expected 'resize <shrink|grow> "
		"<width|height> [<amount>] [px|ppt]'";

	int multiplier = 0;
	if (strcasecmp(*argv, "grow") == 0) {
		multiplier = 1;
	} else if (strcasecmp(*argv, "shrink") == 0) {
		multiplier = -1;
	} else {
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}
	--argc; ++argv;

	enum resize_axis axis = parse_resize_axis(*argv);
	if (axis == RESIZE_AXIS_INVALID) {
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}
	--argc; ++argv;

	int amount = 10; // Default amount
	enum resize_unit unit = RESIZE_UNIT_DEFAULT;

	if (argc) {
		char *err;
		amount = (int)strtol(*argv, &err, 10);
		if (*err) {
			// e.g. `resize grow width 10px`
			unit = parse_resize_unit(err);
			if (unit == RESIZE_UNIT_INVALID) {
				return cmd_results_new(CMD_INVALID, "resize", usage);
			}
		}
		--argc; ++argv;
	}

	if (argc) {
		unit = parse_resize_unit(*argv);
		if (unit == RESIZE_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, "resize", usage);
		}
		--argc; ++argv;
	}

	if (argc) {
		// Provied too many args, the bastard
		return cmd_results_new(CMD_INVALID, "resize", usage);
	}

	resize(amount * multiplier, axis, unit);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
