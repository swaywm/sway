#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include "sway/security.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "ipc.h"
#include "log.h"
#include "util.h"

static struct ipc_policy *current_policy = NULL;

struct cmd_results *cmd_ipc(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "ipc", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}
	if ((error = check_security_config())) {
		return error;
	}
	if (config->reading && strcmp("{", argv[1]) != 0) {
		return cmd_results_new(CMD_INVALID, "ipc",
				"Expected '{' at start of IPC config definition.");
	}
	if (!config->reading) {
		return cmd_results_new(CMD_FAILURE, "ipc", "Can only be used in config file.");
	}

	char *program = NULL;

	if (!(program = resolve_ipc_path(argv[0]))) {
		return cmd_results_new(
				CMD_FAILURE, "ipc", "Memory allocation error occured.");
	}
	current_policy = alloc_ipc_policy(program);
	list_add(config->ipc_policies, current_policy);

	free(program);
	return cmd_results_new(CMD_BLOCK_IPC, NULL, NULL);
}

struct cmd_results *cmd_ipc_events(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "events", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[0]) != 0) {
		return cmd_results_new(CMD_INVALID, "events",
				"Expected '{' at start of IPC event config definition.");
	}

	if (!config->reading) {
		return cmd_results_new(CMD_FAILURE, "events", "Can only be used in config file.");
	}

	return cmd_results_new(CMD_BLOCK_IPC_EVENTS, NULL, NULL);
}

struct cmd_results *cmd_ipc_cmd(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "ipc", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	bool enabled;
	if (strcmp(argv[0], "enabled") == 0) {
		enabled = true;
	} else if (strcmp(argv[0], "disabled") == 0) {
		enabled = false;
	} else {
		return cmd_results_new(CMD_INVALID, argv[-1],
				"Argument must be one of 'enabled' or 'disabled'");
	}

	struct {
		char *name;
		enum ipc_feature type;
	} types[] = {
		{ "*", IPC_FEATURE_ALL_COMMANDS },
		{ "command", IPC_FEATURE_COMMAND },
		{ "workspaces", IPC_FEATURE_GET_WORKSPACES },
		{ "outputs", IPC_FEATURE_GET_OUTPUTS },
		{ "tree", IPC_FEATURE_GET_TREE },
		{ "marks", IPC_FEATURE_GET_MARKS },
		{ "bar-config", IPC_FEATURE_GET_BAR_CONFIG },
		{ "inputs", IPC_FEATURE_GET_INPUTS },
	};

	uint32_t type = 0;

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
		if (strcmp(types[i].name, argv[-1]) == 0) {
			type = types[i].type;
			break;
		}
	}

	if (enabled) {
		current_policy->features |= type;
		sway_log(L_DEBUG, "Enabled IPC %s feature", argv[-1]);
	} else {
		current_policy->features &= ~type;
		sway_log(L_DEBUG, "Disabled IPC %s feature", argv[-1]);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_ipc_event_cmd(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "ipc", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	bool enabled;
	if (strcmp(argv[0], "enabled") == 0) {
		enabled = true;
	} else if (strcmp(argv[0], "disabled") == 0) {
		enabled = false;
	} else {
		return cmd_results_new(CMD_INVALID, argv[-1],
				"Argument must be one of 'enabled' or 'disabled'");
	}

	struct {
		char *name;
		enum ipc_feature type;
	} types[] = {
		{ "*", IPC_FEATURE_ALL_EVENTS },
		{ "workspace", IPC_FEATURE_EVENT_WORKSPACE },
		{ "output", IPC_FEATURE_EVENT_OUTPUT },
		{ "mode", IPC_FEATURE_EVENT_MODE },
		{ "window", IPC_FEATURE_EVENT_WINDOW },
		{ "binding", IPC_FEATURE_EVENT_BINDING },
		{ "input", IPC_FEATURE_EVENT_INPUT },
	};

	uint32_t type = 0;

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
		if (strcmp(types[i].name, argv[-1]) == 0) {
			type = types[i].type;
			break;
		}
	}

	if (enabled) {
		current_policy->features |= type;
		sway_log(L_DEBUG, "Enabled IPC %s event", argv[-1]);
	} else {
		current_policy->features &= ~type;
		sway_log(L_DEBUG, "Disabled IPC %s event", argv[-1]);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
