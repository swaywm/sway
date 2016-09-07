#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
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
	swayc_t *view = get_focused_float(swayc_active_workspace());
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
	swayc_t *view = get_focused_float(swayc_active_workspace());

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
	swayc_t *parent = get_focused_view(swayc_active_workspace());
	swayc_t *focused = parent;
	swayc_t *sibling;
	if (!parent) {
		return true;
	}
	// Find the closest parent container which has siblings of the proper layout.
	// Then apply the resize to all of them.
	int i;
	if (use_width) {
		int lnumber = 0;
		int rnumber = 0;
		while (parent->parent) {
			if (parent->parent->layout == L_HORIZ && parent->parent->children) {
				for (i = 0; i < parent->parent->children->length; i++) {
					sibling = parent->parent->children->items[i];
					if (sibling->x != focused->x) {
						if (sibling->x < parent->x) {
							lnumber++;
						} else if (sibling->x > parent->x) {
							rnumber++;
						}
					}
				}
				if (rnumber || lnumber) {
					break;
				}
			}
			parent = parent->parent;
		}
		if (parent == &root_container) {
			return true;
		}
		sway_log(L_DEBUG, "Found the proper parent: %p. It has %d l conts, and %d r conts", parent->parent, lnumber, rnumber);
		//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
		bool valid = true;
		for (i = 0; i < parent->parent->children->length; i++) {
			sibling = parent->parent->children->items[i];
			if (sibling->x != focused->x) {
				if (sibling->x < parent->x) {
					double pixels = -1 * amount;
					pixels /= lnumber;
					if (rnumber) {
						if ((sibling->width + pixels/2) < min_sane_w) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->width + pixels) < min_sane_w) {
							valid = false;
							break;
						}
					}
				} else if (sibling->x > parent->x) {
					double pixels = -1 * amount;
					pixels /= rnumber;
					if (lnumber) {
						if ((sibling->width + pixels/2) < min_sane_w) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->width + pixels) < min_sane_w) {
							valid = false;
							break;
						}
					}
				}
			} else {
				double pixels = amount;
				if (parent->width + pixels < min_sane_w) {
					valid = false;
					break;
				}
			}
		}
		if (valid) {
			for (i = 0; i < parent->parent->children->length; i++) {
				sibling = parent->parent->children->items[i];
				if (sibling->x != focused->x) {
					if (sibling->x < parent->x) {
						double pixels = -1 * amount;
						pixels /= lnumber;
						if (rnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_RIGHT);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_RIGHT);
						}
					} else if (sibling->x > parent->x) {
						double pixels = -1 * amount;
						pixels /= rnumber;
						if (lnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_LEFT);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_LEFT);
						}
					}
				} else {
					if (rnumber != 0 && lnumber != 0) {
						double pixels = amount;
						pixels /= 2;
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_LEFT);
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_RIGHT);
					} else if (rnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_RIGHT);
					} else if (lnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_LEFT);
					}
				}
			}
			// Recursive resize does not handle positions, let arrange_windows
			// take care of that.
			arrange_windows(swayc_active_workspace(), -1, -1);
		}
		return true;
	} else {
		int tnumber = 0;
		int bnumber = 0;
		while (parent->parent) {
			if (parent->parent->layout == L_VERT) {
				for (i = 0; i < parent->parent->children->length; i++) {
					sibling = parent->parent->children->items[i];
					if (sibling->y != focused->y) {
						if (sibling->y < parent->y) {
							bnumber++;
						} else if (sibling->y > parent->y) {
							tnumber++;
						}
					}
				}
				if (bnumber || tnumber) {
					break;
				}
			}
			parent = parent->parent;
		}
		if (parent->parent == NULL || parent->parent->children == NULL) {
			return true;
		}
		sway_log(L_DEBUG, "Found the proper parent: %p. It has %d b conts, and %d t conts", parent->parent, bnumber, tnumber);
		//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
		bool valid = true;
		for (i = 0; i < parent->parent->children->length; i++) {
			sibling = parent->parent->children->items[i];
			if (sibling->y != focused->y) {
				if (sibling->y < parent->y) {
					double pixels = -1 * amount;
					pixels /= bnumber;
					if (tnumber) {
						if ((sibling->height + pixels/2) < min_sane_h) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->height + pixels) < min_sane_h) {
							valid = false;
							break;
						}
					}
				} else if (sibling->y > parent->y) {
					double pixels = -1 * amount;
					pixels /= tnumber;
					if (bnumber) {
						if ((sibling->height + pixels/2) < min_sane_h) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->height + pixels) < min_sane_h) {
							valid = false;
							break;
						}
					}
				}
			} else {
				double pixels = amount;
				if (parent->height + pixels < min_sane_h) {
					valid = false;
					break;
				}
			}
		}
		if (valid) {
			for (i = 0; i < parent->parent->children->length; i++) {
				sibling = parent->parent->children->items[i];
				if (sibling->y != focused->y) {
					if (sibling->y < parent->y) {
						double pixels = -1 * amount;
						pixels /= bnumber;
						if (tnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_BOTTOM);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_BOTTOM);
						}
					} else if (sibling->x > parent->x) {
						double pixels = -1 * amount;
						pixels /= tnumber;
						if (bnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_TOP);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_TOP);
						}
					}
				} else {
					if (bnumber != 0 && tnumber != 0) {
						double pixels = amount/2;
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_TOP);
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_BOTTOM);
					} else if (tnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_TOP);
					} else if (bnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_BOTTOM);
					}
				}
			}
			arrange_windows(swayc_active_workspace(), -1, -1);
		}
		return true;
	}
	return true;
}

static bool set_size_tiled(int amount, bool use_width) {
	int desired;
	swayc_t *focused = get_focused_view(swayc_active_workspace());

	if (use_width) {
		desired = amount - focused->width;
	} else {
		desired = amount - focused->height;
	}

	return resize_tiled(desired, use_width);
}

static bool set_size(int dimension, bool use_width) {
	swayc_t *focused = get_focused_view_include_floating(swayc_active_workspace());

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
	swayc_t *focused = get_focused_view_include_floating(swayc_active_workspace());

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
