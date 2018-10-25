#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/seat.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

static struct workspace_config *workspace_config_find_or_create(char *ws_name) {
	struct workspace_config *wsc = workspace_find_config(ws_name);
	if (wsc) {
		return wsc;
	}
	wsc = calloc(1, sizeof(struct workspace_config));
	if (!wsc) {
		return NULL;
	}
	wsc->workspace = strdup(ws_name);
	wsc->gaps_inner = INT_MIN;
	wsc->gaps_outer = INT_MIN;
	list_add(config->workspace_configs, wsc);
	return wsc;
}

void free_workspace_config(struct workspace_config *wsc) {
	free(wsc->workspace);
	free(wsc->output);
	free(wsc);
}

struct cmd_results *cmd_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	int output_location = -1;
	int gaps_location = -1;

	for (int i = 0; i < argc; ++i) {
		if (strcasecmp(argv[i], "output") == 0) {
			output_location = i;
			break;
		}
	}
	for (int i = 0; i < argc; ++i) {
		if (strcasecmp(argv[i], "gaps") == 0) {
			gaps_location = i;
			break;
		}
	}
	if (output_location >= 0) {
		if ((error = checkarg(argc, "workspace", EXPECTED_EQUAL_TO, output_location + 2))) {
			return error;
		}
		char *ws_name = join_args(argv, argc - 2);
		struct workspace_config *wsc = workspace_config_find_or_create(ws_name);
		free(ws_name);
		if (!wsc) {
			return cmd_results_new(CMD_FAILURE, "workspace output",
					"Unable to allocate workspace output");
		}
		free(wsc->output);
		wsc->output = strdup(argv[output_location + 1]);
	} else if (gaps_location >= 0) {
		if ((error = checkarg(argc, "workspace", EXPECTED_EQUAL_TO, gaps_location + 3))) {
			return error;
		}
		char *ws_name = join_args(argv, argc - 3);
		struct workspace_config *wsc = workspace_config_find_or_create(ws_name);
		free(ws_name);
		if (!wsc) {
			return cmd_results_new(CMD_FAILURE, "workspace gaps",
					"Unable to allocate workspace output");
		}
		int *prop = NULL;
		if (strcasecmp(argv[gaps_location + 1], "inner") == 0) {
			prop = &wsc->gaps_inner;
		} else if (strcasecmp(argv[gaps_location + 1], "outer") == 0) {
			prop = &wsc->gaps_outer;
		} else {
			return cmd_results_new(CMD_FAILURE, "workspace gaps",
					"Expected 'workspace <ws> gaps inner|outer <px>'");
		}
		char *end;
		int val = strtol(argv[gaps_location + 2], &end, 10);

		if (strlen(end)) {
			free(end);
			return cmd_results_new(CMD_FAILURE, "workspace gaps",
					"Expected 'workspace <ws> gaps inner|outer <px>'");
		}
		*prop = val;

		// Prevent invalid gaps configurations.
		if (wsc->gaps_inner < 0) {
			wsc->gaps_inner = 0;
		}
		if (wsc->gaps_outer < -wsc->gaps_inner) {
			wsc->gaps_outer = -wsc->gaps_inner;
		}

	} else {
		if (config->reading || !config->active) {
			return cmd_results_new(CMD_DEFER, "workspace", NULL);
		} else if (!root->outputs->length) {
			return cmd_results_new(CMD_INVALID, "workspace",
					"Can't run this command while there's no outputs connected.");
		}

		bool no_auto_back_and_forth = false;
		while (strcasecmp(argv[0], "--no-auto-back-and-forth") == 0) {
			no_auto_back_and_forth = true;
			if ((error = checkarg(--argc, "workspace", EXPECTED_AT_LEAST, 1))) {
				return error;
			}
			++argv;
		}


		struct sway_workspace *ws = NULL;
		if (strcasecmp(argv[0], "number") == 0) {
			if (argc < 2) {
				return cmd_results_new(CMD_INVALID, "workspace",
						"Expected workspace number");
			}
			if (!isdigit(argv[1][0])) {
				return cmd_results_new(CMD_INVALID, "workspace",
						"Invalid workspace number '%s'", argv[1]);
			}
			if (!(ws = workspace_by_number(argv[1]))) {
				char *name = join_args(argv + 1, argc - 1);
				ws = workspace_create(NULL, name);
				free(name);
			}
		} else if (strcasecmp(argv[0], "next") == 0 ||
				strcasecmp(argv[0], "prev") == 0 ||
				strcasecmp(argv[0], "next_on_output") == 0 ||
				strcasecmp(argv[0], "prev_on_output") == 0 ||
				strcasecmp(argv[0], "current") == 0) {
			ws = workspace_by_name(argv[0]);
		} else if (strcasecmp(argv[0], "back_and_forth") == 0) {
			struct sway_seat *seat = config->handler_context.seat;
			if (!seat->prev_workspace_name) {
				return cmd_results_new(CMD_INVALID, "workspace",
						"There is no previous workspace");
			}
			if (!(ws = workspace_by_name(argv[0]))) {
				ws = workspace_create(NULL, seat->prev_workspace_name);
			}
		} else {
			char *name = join_args(argv, argc);
			if (!(ws = workspace_by_name(name))) {
				ws = workspace_create(NULL, name);
			}
			free(name);
		}
		workspace_switch(ws, no_auto_back_and_forth);
		seat_consider_warp_to_focus(config->handler_context.seat);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
