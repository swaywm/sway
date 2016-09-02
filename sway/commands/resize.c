#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/resize.h"
#include "log.h"

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
