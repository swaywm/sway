#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "stringop.h"
#include <math.h>

enum gaps_op {
	GAPS_OP_SET,
	GAPS_OP_ADD,
	GAPS_OP_SUBTRACT
};

struct gaps_data {
	bool inner;
	enum gaps_op operation;
	int amount;
};

// gaps inner|outer <px>
static struct cmd_results *gaps_set_defaults(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 2);
	if (error) {
		return error;
	}

	bool inner;
	if (strcasecmp(argv[0], "inner") == 0) {
		inner = true;
	} else if (strcasecmp(argv[0], "outer") == 0) {
		inner = false;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer <px>'");
	}

	char *end;
	int amount = strtol(argv[1], &end, 10);
	if (strlen(end) && strcasecmp(end, "px") != 0) {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer <px>'");
	}
	if (inner) {
		config->gaps_inner = (amount >= 0) ? amount : 0;
	} else {
		config->gaps_outer = amount;
	}

	// Prevent negative outer gaps from moving windows out of the workspace.
	if (config->gaps_outer < -config->gaps_inner) {
		config->gaps_outer = -config->gaps_inner;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static void configure_gaps(struct sway_workspace *ws, void *_data) {
	struct gaps_data *data = _data;
	int *prop = data->inner ? &ws->gaps_inner : &ws->gaps_outer;

	switch (data->operation) {
	case GAPS_OP_SET:
		*prop = data->amount;
		break;
	case GAPS_OP_ADD:
		*prop += data->amount;
		break;
	case GAPS_OP_SUBTRACT:
		*prop -= data->amount;
		break;
	}
	// Prevent invalid gaps configurations.
	if (ws->gaps_inner < 0) {
		ws->gaps_inner = 0;
	}
	if (ws->gaps_outer < -ws->gaps_inner) {
		ws->gaps_outer = -ws->gaps_inner;
	}
	arrange_workspace(ws);
}

// gaps inner|outer current|all set|plus|minus <px>
static struct cmd_results *gaps_set_runtime(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 4);
	if (error) {
		return error;
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Can't run this command while there's no outputs connected.");
	}

	struct gaps_data data;

	if (strcasecmp(argv[0], "inner") == 0) {
		data.inner = true;
	} else if (strcasecmp(argv[0], "outer") == 0) {
		data.inner = false;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer current|all set|plus|minus <px>'");
	}

	bool all;
	if (strcasecmp(argv[1], "current") == 0) {
		all = false;
	} else if (strcasecmp(argv[1], "all") == 0) {
		all = true;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer current|all set|plus|minus <px>'");
	}

	if (strcasecmp(argv[2], "set") == 0) {
		data.operation = GAPS_OP_SET;
	} else if (strcasecmp(argv[2], "plus") == 0) {
		data.operation = GAPS_OP_ADD;
	} else if (strcasecmp(argv[2], "minus") == 0) {
		data.operation = GAPS_OP_SUBTRACT;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer current|all set|plus|minus <px>'");
	}

	char *end;
	data.amount = strtol(argv[3], &end, 10);
	if (strlen(end) && strcasecmp(end, "px") != 0) {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer current|all set|plus|minus <px>'");
	}

	if (all) {
		root_for_each_workspace(configure_gaps, &data);
	} else {
		configure_gaps(config->handler_context.workspace, &data);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

// gaps inner|outer <px> - sets defaults for workspaces
// gaps inner|outer current|all set|plus|minus <px> - runtime only
struct cmd_results *cmd_gaps(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 2);
	if (error) {
		return error;
	}

	bool config_loading = !config->active || config->reloading;

	if (argc == 2) {
		return gaps_set_defaults(argc, argv);
	}
	if (argc == 4 && !config_loading) {
		return gaps_set_runtime(argc, argv);
	}
	if (config_loading) {
		return cmd_results_new(CMD_INVALID, "gaps",
				"Expected 'gaps inner|outer <px>'");
	}
	return cmd_results_new(CMD_INVALID, "gaps",
			"Expected 'gaps inner|outer <px>' or "
			"'gaps inner|outer current|all set|plus|minus <px>'");
}
