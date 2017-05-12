#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlc/wlc.h>
#include "sway/commands.h"
#include "sway/layout.h"
#include "sway/focus.h"
#include "sway/input_state.h"
#include "sway/handlers.h"
#include "log.h"

enum resize_dim_types {
	RESIZE_DIM_PX,
	RESIZE_DIM_PPT,
	RESIZE_DIM_DEFAULT,
};

static bool set_size_floating(int new_dimension, bool use_width) {
	swayc_t *view = current_container;
	if (view) {
		if (use_width) {
			int current_width = view->width;
			view->desired_width = new_dimension;
			floating_view_sane_size(view);

			int new_x = view->x + (int)(((view->desired_width - current_width) / 2) * -1);
			view->width = view->desired_width;
			view->x = new_x;

			update_geometry(view);
		} else {
			int current_height = view->height;
			view->desired_height = new_dimension;
			floating_view_sane_size(view);

			int new_y = view->y + (int)(((view->desired_height - current_height) / 2) * -1);
			view->height = view->desired_height;
			view->y = new_y;

			update_geometry(view);
		}

		return true;
	}

	return false;
}

static bool resize_floating(int amount, bool use_width) {
	swayc_t *view = current_container;

	if (view) {
		if (use_width) {
			return set_size_floating(view->width + amount, true);
		} else {
			return set_size_floating(view->height + amount, false);
		}
	}

	return false;
}

static bool resize_tiled(int amount, bool use_width) {
	swayc_t *container = current_container;
	swayc_t *parent = container->parent;
	int idx_focused = 0;
	bool use_major = false;
	size_t nb_before = 0;
	size_t nb_after = 0;

	// 1. Identify a container ancestor that will allow the focused child to grow in the requested
	//    direction.
	while (container->parent) {
		parent = container->parent;
		if ((parent->children && parent->children->length > 1)
			&& (is_auto_layout(parent->layout)
				|| (use_width ? parent->layout == L_HORIZ : parent->layout == L_VERT))) {
			// check if container has siblings that can provide/absorb the space needed for
			// the resize operation.
			use_major = use_width
				? parent->layout == L_AUTO_LEFT || parent->layout == L_AUTO_RIGHT
				: parent->layout == L_AUTO_TOP || parent->layout == L_AUTO_BOTTOM;
			// Note: use_major will be false for L_HORIZ and L_VERT

			idx_focused = index_child(container);
			if (idx_focused < 0) {
				sway_log(L_ERROR, "Something weird is happening, child container not "
					 "present in its parent's children list.");
				continue;
			}
			if (use_major) {
				nb_before = auto_group_index(parent, idx_focused);
				nb_after = auto_group_count(parent) - nb_before - 1;
			} else {
				nb_before = idx_focused - auto_group_start_index(parent, idx_focused);
				nb_after = auto_group_end_index(parent, idx_focused) - idx_focused - 1;
				sway_log(L_DEBUG, "+++ focused: %d, start: %d, end: %d, before: %d, after: %d",
					 idx_focused,
					 (int)auto_group_start_index(parent, idx_focused),
					 (int)auto_group_end_index(parent, idx_focused),
					 (int)nb_before, (int)nb_after);

			}
			if (nb_before || nb_after) {
				break;
			}
		}
		container = parent; /* continue up the tree to the next ancestor */
	}
	if (parent == &root_container) {
		return true;
	}
	sway_log(L_DEBUG, "Found the proper parent: %p. It has %zu before conts, "
		 "and %zu after conts", parent, nb_before, nb_after);
	// 2. Ensure that the resize operation will not make one of the resized containers drop
	//    below the "sane" size threshold.
	bool valid = true;
	swayc_t *focused = *(swayc_t **)list_get(parent->children, idx_focused);
	size_t start = use_major ? 0 : (size_t)auto_group_start_index(parent, idx_focused);
	size_t end = use_major ? parent->children->length : (size_t)auto_group_end_index(parent, idx_focused);
	sway_log(L_DEBUG, "Check children of container %p [%zu,%zu[", container, start, end);
	for (size_t i = start; i < end; ) {
		swayc_t *sibling = *(swayc_t **)list_get(parent->children, i);
		double pixels = amount;
		bool is_before = use_width ? sibling->x < focused->x : sibling->y < focused->y;
		bool is_after  = use_width ? sibling->x > focused->x : sibling->y > focused->y;
		if (is_before || is_after) {
			pixels = -pixels;
			pixels /= is_before ? nb_before : nb_after;
			if (nb_after != 0 && nb_before != 0) {
				pixels /= 2;
			}
		}
		sway_log(L_DEBUG, "Check container %p: width %g vs %d, height %g vs %d", sibling, sibling->width + pixels, min_sane_w, sibling->height + pixels, min_sane_h);
		if (use_width ?
			sibling->width + pixels < min_sane_w :
			sibling->height + pixels < min_sane_h) {
			valid = false;
			sway_log(L_DEBUG, "Container size no longer sane");
			break;
		}
		i = use_major ? (size_t)auto_group_end_index(parent, i) : (i + 1);
		sway_log(L_DEBUG, "+++++ check %zu", i);
	}
	// 3. Apply the size change
	if (valid) {
		for (size_t i = start; i < end; ) {
			int next_i = use_major ? (size_t)auto_group_end_index(parent, i) : (i + 1);
			swayc_t *sibling = *(swayc_t **)list_get(parent->children, i);
			double pixels = amount;
			bool is_before = use_width ? sibling->x < focused->x : sibling->y < focused->y;
			bool is_after  = use_width ? sibling->x > focused->x : sibling->y > focused->y;
			if (is_before || is_after) {
				pixels = -pixels;
				pixels /= is_before ? nb_before : nb_after;
				if (nb_after != 0 && nb_before != 0) {
					pixels /= 2;
				}
				sway_log(L_DEBUG, "%p: %s", sibling, is_before ? "before" : "after");
				if (use_major) {
					for (int j = i; j < next_i; ++j) {
						swayc_t *item = *(swayc_t **)list_get(parent->children, j);
						recursive_resize(item, pixels,
								 use_width ?
								 (is_before ? WLC_RESIZE_EDGE_RIGHT : WLC_RESIZE_EDGE_LEFT) :
								 (is_before ? WLC_RESIZE_EDGE_BOTTOM : WLC_RESIZE_EDGE_TOP));
					}
				} else {
					recursive_resize(sibling, pixels,
							use_width ?
							(is_before ? WLC_RESIZE_EDGE_RIGHT : WLC_RESIZE_EDGE_LEFT) :
							(is_before ? WLC_RESIZE_EDGE_BOTTOM : WLC_RESIZE_EDGE_TOP));
				}
			} else {
				if (use_major) {
					for (int j = i; j < next_i; ++j) {
						swayc_t *item = *(swayc_t **)list_get(parent->children, j);
						recursive_resize(item, pixels / 2,
								 use_width ? WLC_RESIZE_EDGE_LEFT : WLC_RESIZE_EDGE_TOP);
						recursive_resize(item, pixels / 2,
								 use_width ? WLC_RESIZE_EDGE_RIGHT : WLC_RESIZE_EDGE_BOTTOM);
					}
				} else {
					recursive_resize(sibling, pixels / 2,
							 use_width ? WLC_RESIZE_EDGE_LEFT : WLC_RESIZE_EDGE_TOP);
					recursive_resize(sibling, pixels / 2,
							 use_width ? WLC_RESIZE_EDGE_RIGHT : WLC_RESIZE_EDGE_BOTTOM);
				}
			}
			i = next_i;
		}
		// Recursive resize does not handle positions, let arrange_windows
		// take care of that.
		arrange_windows(swayc_active_workspace(), -1, -1);
	}
	return true;
}

static bool set_size_tiled(int amount, bool use_width) {
	int desired;
	swayc_t *focused = current_container;

	if (use_width) {
		desired = amount - focused->width;
	} else {
		desired = amount - focused->height;
	}

	return resize_tiled(desired, use_width);
}

static bool set_size(int dimension, bool use_width) {
	swayc_t *focused = current_container;

	if (focused) {
		if (focused->is_floating) {
			return set_size_floating(dimension, use_width);
		} else {
			return set_size_tiled(dimension, use_width);
		}
	}

	return false;
}

static bool resize(int dimension, bool use_width, enum resize_dim_types dim_type) {
	swayc_t *focused = current_container;

	// translate "10 ppt" (10%) to appropriate # of pixels in case we need it
	float ppt_dim = (float)dimension / 100;

	if (use_width) {
		ppt_dim = focused->width * ppt_dim;
	} else {
		ppt_dim = focused->height * ppt_dim;
	}

	if (focused) {
		if (focused->is_floating) {
			// floating view resize dimensions should default to px, so only
			// use ppt if specified
			if (dim_type == RESIZE_DIM_PPT) {
				dimension = (int)ppt_dim;
			}

			return resize_floating(dimension, use_width);
		} else {
			// tiled view resize dimensions should default to ppt, so only use
			// px if specified
			if (dim_type != RESIZE_DIM_PX) {
				dimension = (int)ppt_dim;
			}

			return resize_tiled(dimension, use_width);
		}
	}

	return false;
}

static struct cmd_results *cmd_resize_set(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "resize set", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (strcasecmp(argv[0], "width") == 0 || strcasecmp(argv[0], "height") == 0) {
		// handle `reset set width 100 px height 100 px` syntax, also allows
		// specifying only one dimension for a `resize set`
		int cmd_num = 0;
		int dim;

		while ((cmd_num + 1) < argc) {
			dim = (int)strtol(argv[cmd_num + 1], NULL, 10);
			if (errno == ERANGE || dim == 0) {
				errno = 0;
				return cmd_results_new(CMD_INVALID, "resize set",
					"Expected 'resize set <width|height> <amount> [px] [<width|height> <amount> [px]]'");
			}

			if (strcasecmp(argv[cmd_num], "width") == 0) {
				set_size(dim, true);
			} else if (strcasecmp(argv[cmd_num], "height") == 0) {
				set_size(dim, false);
			} else {
				return cmd_results_new(CMD_INVALID, "resize set",
					"Expected 'resize set <width|height> <amount> [px] [<width|height> <amount> [px]]'");
			}

			cmd_num += 2;

			if (cmd_num < argc && strcasecmp(argv[cmd_num], "px") == 0) {
				// if this was `resize set width 400 px height 300 px`, disregard the `px` arg
				cmd_num++;
			}
		}
	} else {
		// handle `reset set 100 px 100 px` syntax
		int width = (int)strtol(argv[0], NULL, 10);
		if (errno == ERANGE || width == 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "resize set",
				"Expected 'resize set <width> [px] <height> [px]'");
		}

		int height_arg = 1;
		if (strcasecmp(argv[1], "px") == 0) {
			height_arg = 2;
		}

		int height = (int)strtol(argv[height_arg], NULL, 10);
		if (errno == ERANGE || height == 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "resize set",
				"Expected 'resize set <width> [px] <height> [px]'");
		}

		set_size(width, true);
		set_size(height, false);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_resize(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "resize", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "resize", "Can only be used when sway is running.");

	if (strcasecmp(argv[0], "set") == 0) {
		return cmd_resize_set(argc - 1, &argv[1]);
	}

	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	int dim_arg = argc - 1;

	enum resize_dim_types dim_type = RESIZE_DIM_DEFAULT;
	if (strcasecmp(argv[dim_arg], "ppt") == 0) {
		dim_type = RESIZE_DIM_PPT;
		dim_arg--;
	} else if (strcasecmp(argv[dim_arg], "px") == 0) {
		dim_type = RESIZE_DIM_PX;
		dim_arg--;
	}

	int amount = (int)strtol(argv[dim_arg], NULL, 10);
	if (errno == ERANGE || amount == 0) {
		errno = 0;
		amount = 10; // this is the default resize dimension used by i3 for both px and ppt
		sway_log(L_DEBUG, "Tried to get resize dimension out of '%s' but failed; setting dimension to default %d",
			argv[dim_arg], amount);
	}

	bool use_width = false;
	if (strcasecmp(argv[1], "width") == 0) {
		use_width = true;
	} else if (strcasecmp(argv[1], "height") != 0) {
		return cmd_results_new(CMD_INVALID, "resize",
			"Expected 'resize <shrink|grow> <width|height> [<amount>] [px|ppt]'");
	}

	if (strcasecmp(argv[0], "shrink") == 0) {
		amount *= -1;
	} else if (strcasecmp(argv[0], "grow") != 0) {
		return cmd_results_new(CMD_INVALID, "resize",
			"Expected 'resize <shrink|grow> <width|height> [<amount>] [px|ppt]'");
	}

	resize(amount, use_width, dim_type);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
