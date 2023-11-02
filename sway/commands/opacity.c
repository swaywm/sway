#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/view.h"
#include "log.h"

struct cmd_results *cmd_opacity(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "opacity", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct sway_container *con = config->handler_context.container;

	if (con == NULL) {
		return cmd_results_new(CMD_FAILURE, "No current container");
	}

	enum { SET, PLUS, MINUS, MUL, DIV } op = SET;
	char *argval = argv[0];

	if (argc == 1) {
		if (!strcasecmp(argv[0], "get")) {
			return cmd_results_new(CMD_SUCCESS, "%f", con->alpha);
		}
	} else {
		if ((error = checkarg(argc, "opacity", EXPECTED_EQUAL_TO, 2))) {
			return error;
		}
		if (!strcasecmp(argv[0], "set")) {
			op = SET;
		} else if (!strcasecmp(argv[0], "plus")) {
			op = PLUS;
		} else if (!strcasecmp(argv[0], "minus")) {
			op = MINUS;
		} else if (!strcasecmp(argv[0], "mul")) {
			op = MUL;
		} else if (!strcasecmp(argv[0], "div")) {
			op = DIV;
		} else {
			return cmd_results_new(CMD_INVALID,
				"Expected: get,set|plus|minus|mul|div <0..1>: %s", argv[0]);
		}
		argval = argv[1];
	}

	char *err;
	float val = strtof(argval, &err);
	if (*err) {
		return cmd_results_new(CMD_INVALID, "opacity float invalid");
	}

	switch (op) {
		case SET: break;
		case PLUS: val += con->alpha; break;
		case MINUS: val -= con->alpha; break;
		case MUL: val *= con->alpha; break;
		case DIV: val = con->alpha / val; break;
	}

	if (val < 0 || val > 1) {
		return cmd_results_new(CMD_FAILURE, "opacity value out of bounds");
	}

	con->alpha = val;
	container_damage_whole(con);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
