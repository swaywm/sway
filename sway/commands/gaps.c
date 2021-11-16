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
	GAPS_OP_SUBTRACT,
	GAPS_OP_TOGGLE
};

struct gaps_data {
	bool inner;
	struct {
		bool top;
		bool right;
		bool bottom;
		bool left;
	} outer;
	enum gaps_op operation;
	int amount;
};

// Prevent negative outer gaps from moving windows out of the workspace.
static void prevent_invalid_outer_gaps(void) {
	if (config->gaps_outer.top < -config->gaps_inner) {
		config->gaps_outer.top = -config->gaps_inner;
	}
	if (config->gaps_outer.right < -config->gaps_inner) {
		config->gaps_outer.right = -config->gaps_inner;
	}
	if (config->gaps_outer.bottom < -config->gaps_inner) {
		config->gaps_outer.bottom = -config->gaps_inner;
	}
	if (config->gaps_outer.left < -config->gaps_inner) {
		config->gaps_outer.left = -config->gaps_inner;
	}
}

// gaps inner|outer|horizontal|vertical|top|right|bottom|left <px>
static const char expected_defaults[] =
	"'gaps inner|outer|horizontal|vertical|top|right|bottom|left <px>'";
static struct cmd_results *gaps_set_defaults(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 2);
	if (error) {
		return error;
	}

	char *end;
	int amount = strtol(argv[1], &end, 10);
	if (strlen(end) && strcasecmp(end, "px") != 0) {
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_defaults);
	}

	bool valid = false;
	if (!strcasecmp(argv[0], "inner")) {
		valid = true;
		config->gaps_inner = (amount >= 0) ? amount : 0;
	} else {
		if (!strcasecmp(argv[0], "outer") || !strcasecmp(argv[0], "vertical")
				|| !strcasecmp(argv[0], "top")) {
			valid = true;
			config->gaps_outer.top = amount;
		}
		if (!strcasecmp(argv[0], "outer") || !strcasecmp(argv[0], "horizontal")
				|| !strcasecmp(argv[0], "right")) {
			valid = true;
			config->gaps_outer.right = amount;
		}
		if (!strcasecmp(argv[0], "outer") || !strcasecmp(argv[0], "vertical")
				|| !strcasecmp(argv[0], "bottom")) {
			valid = true;
			config->gaps_outer.bottom = amount;
		}
		if (!strcasecmp(argv[0], "outer") || !strcasecmp(argv[0], "horizontal")
				|| !strcasecmp(argv[0], "left")) {
			valid = true;
			config->gaps_outer.left = amount;
		}
	}
	if (!valid) {
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_defaults);
	}

	prevent_invalid_outer_gaps();
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static void apply_gaps_op(int *prop, enum gaps_op op, int amount) {
	switch (op) {
	case GAPS_OP_SET:
		*prop = amount;
		break;
	case GAPS_OP_ADD:
		*prop += amount;
		break;
	case GAPS_OP_SUBTRACT:
		*prop -= amount;
		break;
	case GAPS_OP_TOGGLE:
		*prop = *prop ? 0 : amount;
		break;
	}
}

static void configure_gaps(struct sway_workspace *ws, void *_data) {
	// Apply operation to gaps
	struct gaps_data *data = _data;
	if (data->inner) {
		apply_gaps_op(&ws->gaps_inner, data->operation, data->amount);
	}
	if (data->outer.top) {
		apply_gaps_op(&(ws->gaps_outer.top), data->operation, data->amount);
	}
	if (data->outer.right) {
		apply_gaps_op(&(ws->gaps_outer.right), data->operation, data->amount);
	}
	if (data->outer.bottom) {
		apply_gaps_op(&(ws->gaps_outer.bottom), data->operation, data->amount);
	}
	if (data->outer.left) {
		apply_gaps_op(&(ws->gaps_outer.left), data->operation, data->amount);
	}

	// Prevent invalid gaps configurations.
	if (ws->gaps_inner < 0) {
		ws->gaps_inner = 0;
	}
	prevent_invalid_outer_gaps();
	arrange_workspace(ws);
}

// gaps inner|outer|horizontal|vertical|top|right|bottom|left current|all
// set|plus|minus|toggle <px>
static const char expected_runtime[] = "'gaps inner|outer|horizontal|vertical|"
	"top|right|bottom|left current|all set|plus|minus|toggle <px>'";
static struct cmd_results *gaps_set_runtime(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 4);
	if (error) {
		return error;
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}

	struct gaps_data data = {0};

	if (strcasecmp(argv[0], "inner") == 0) {
		data.inner = true;
	} else {
		data.outer.top = !strcasecmp(argv[0], "outer") ||
			!strcasecmp(argv[0], "vertical") || !strcasecmp(argv[0], "top");
		data.outer.right = !strcasecmp(argv[0], "outer") ||
			!strcasecmp(argv[0], "horizontal") || !strcasecmp(argv[0], "right");
		data.outer.bottom = !strcasecmp(argv[0], "outer") ||
			!strcasecmp(argv[0], "vertical") || !strcasecmp(argv[0], "bottom");
		data.outer.left = !strcasecmp(argv[0], "outer") ||
			!strcasecmp(argv[0], "horizontal") || !strcasecmp(argv[0], "left");
	}
	if (!data.inner && !data.outer.top && !data.outer.right &&
			!data.outer.bottom && !data.outer.left) {
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_runtime);
	}

	bool all;
	if (strcasecmp(argv[1], "current") == 0) {
		all = false;
	} else if (strcasecmp(argv[1], "all") == 0) {
		all = true;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_runtime);
	}

	if (strcasecmp(argv[2], "set") == 0) {
		data.operation = GAPS_OP_SET;
	} else if (strcasecmp(argv[2], "plus") == 0) {
		data.operation = GAPS_OP_ADD;
	} else if (strcasecmp(argv[2], "minus") == 0) {
		data.operation = GAPS_OP_SUBTRACT;
	} else if (strcasecmp(argv[2], "toggle") == 0) {
		data.operation = GAPS_OP_TOGGLE;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_runtime);
	}

	char *end;
	data.amount = strtol(argv[3], &end, 10);
	if (strlen(end) && strcasecmp(end, "px") != 0) {
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_runtime);
	}

	if (all) {
		root_for_each_workspace(configure_gaps, &data);
	} else {
		configure_gaps(config->handler_context.workspace, &data);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

// gaps inner|outer|<dir>|<side> <px> - sets defaults for workspaces
// gaps inner|outer|<dir>|<side> current|all set|plus|minus|toggle <px> - runtime only
// <dir> = horizontal|vertical
// <side> = top|right|bottom|left
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
		return cmd_results_new(CMD_INVALID, "Expected %s", expected_defaults);
	}
	return cmd_results_new(CMD_INVALID, "Expected %s or %s",
			expected_runtime, expected_defaults);
}
