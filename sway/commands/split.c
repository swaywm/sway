#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "log.h"

static struct cmd_results *do_split(int layout) {
	struct sway_container *con = config->handler_context.current_container;
	struct sway_container *parent = container_split(con, layout);
	container_create_notify(parent);
	arrange_windows(parent, -1, -1);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_split(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "split", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "v") == 0 || strcasecmp(argv[0], "vertical") == 0) {
		do_split(L_VERT);
	} else if (strcasecmp(argv[0], "h") == 0 ||
			strcasecmp(argv[0], "horizontal") == 0) {
		do_split(L_HORIZ);
	} else if (strcasecmp(argv[0], "t") == 0 ||
			strcasecmp(argv[0], "toggle") == 0) {
		struct sway_container *focused =
			config->handler_context.current_container;

		if (focused->parent->layout == L_VERT) {
			do_split(L_HORIZ);
		} else {
			do_split(L_VERT);
		}
	} else {
		error = cmd_results_new(CMD_FAILURE, "split",
			"Invalid split command (expected either horizontal or vertical).");
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_splitv(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "splitv", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	return do_split(L_VERT);
}

struct cmd_results *cmd_splith(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "splitv", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	return do_split(L_HORIZ);
}

struct cmd_results *cmd_splitt(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "splitv", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}

	struct sway_container *con = config->handler_context.current_container;

	if (con->parent->layout == L_VERT) {
		return do_split(L_HORIZ);
	} else {
		return do_split(L_VERT);
	}
}
