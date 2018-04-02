#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "log.h"

static struct cmd_results *_do_split(int argc, char **argv, int layout) {
	char *name = layout == L_VERT  ? "splitv" :
		layout == L_HORIZ ? "splith" : "split";
	struct cmd_results *error = NULL;
	if (config->reading) {
		return cmd_results_new(CMD_FAILURE, name,
				"Can't be used in config file.");
	}
	if (!config->active) {
		return cmd_results_new(CMD_FAILURE, name,
				"Can only be used when sway is running.");
	}
	if ((error = checkarg(argc, name, EXPECTED_EQUAL_TO, 0))) {
		return error;
	}

	struct sway_container *focused = config->handler_context.current_container;
	struct sway_container *parent = container_split(focused, layout);
	arrange_windows(parent, -1, -1);

	// TODO borders: update borders

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_split(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) {
		return cmd_results_new(CMD_FAILURE, "split",
				"Can't be used in config file.");
	}
	if (!config->active) {
		return cmd_results_new(CMD_FAILURE, "split",
				"Can only be used when sway is running.");
	}
	if ((error = checkarg(argc, "split", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "v") == 0 || strcasecmp(argv[0], "vertical") == 0) {
		_do_split(argc - 1, argv + 1, L_VERT);
	} else if (strcasecmp(argv[0], "h") == 0 ||
			strcasecmp(argv[0], "horizontal") == 0) {
		_do_split(argc - 1, argv + 1, L_HORIZ);
	} else if (strcasecmp(argv[0], "t") == 0 ||
			strcasecmp(argv[0], "toggle") == 0) {
		struct sway_container *focused =
			config->handler_context.current_container;

		if (focused->parent->layout == L_VERT) {
			_do_split(argc - 1, argv + 1, L_HORIZ);
		} else {
			_do_split(argc - 1, argv + 1, L_VERT);
		}
	} else {
		error = cmd_results_new(CMD_FAILURE, "split",
			"Invalid split command (expected either horizontal or vertical).");
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_splitv(int argc, char **argv) {
	return _do_split(argc, argv, L_VERT);
}

struct cmd_results *cmd_splith(int argc, char **argv) {
	return _do_split(argc, argv, L_HORIZ);
}

struct cmd_results *cmd_splitt(int argc, char **argv) {
	struct sway_container *focused = config->handler_context.current_container;
	if (focused->parent->layout == L_VERT) {
		return _do_split(argc, argv, L_HORIZ);
	} else {
		return _do_split(argc, argv, L_VERT);
	}
}
