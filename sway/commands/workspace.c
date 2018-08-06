#define _XOPEN_SOURCE 500
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/seat.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	int output_location = -1;

	for (int i = 0; i < argc; ++i) {
		if (strcasecmp(argv[i], "output") == 0) {
			output_location = i;
			break;
		}
	}
	if (output_location >= 0) {
		if ((error = checkarg(argc, "workspace", EXPECTED_EQUAL_TO, output_location + 2))) {
			return error;
		}
		struct workspace_output *wso = calloc(1, sizeof(struct workspace_output));
		if (!wso) {
			return cmd_results_new(CMD_FAILURE, "workspace output",
					"Unable to allocate workspace output");
		}
		wso->workspace = join_args(argv, argc - 2);
		wso->output = strdup(argv[output_location + 1]);
		int i = -1;
		if ((i = list_seq_find(config->workspace_outputs, workspace_output_cmp_workspace, wso)) != -1) {
			struct workspace_output *old = config->workspace_outputs->items[i];
			free(old); // workspaces can only be assigned to a single output
			list_del(config->workspace_outputs, i);
		}
		wlr_log(WLR_DEBUG, "Assigning workspace %s to output %s", wso->workspace, wso->output);
		list_add(config->workspace_outputs, wso);
	} else {
		if (config->reading || !config->active) {
			return cmd_results_new(CMD_DEFER, "workspace", NULL);
		}

		bool no_auto_back_and_forth = false;
		while (strcasecmp(argv[0], "--no-auto-back-and-forth") == 0) {
			no_auto_back_and_forth = true;
			if ((error = checkarg(--argc, "workspace", EXPECTED_AT_LEAST, 1))) {
				return error;
			}
			++argv;
		}


		struct sway_container *ws = NULL;
		if (strcasecmp(argv[0], "number") == 0) {
			if (argc < 2) {
				cmd_results_new(CMD_INVALID, "workspace",
						"Expected workspace number");
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
			if (!(ws = workspace_by_name(argv[0])) && prev_workspace_name) {
				ws = workspace_create(NULL, prev_workspace_name);
			}
		} else {
			char *name = join_args(argv, argc);
			if (!(ws = workspace_by_name(name))) {
				ws = workspace_create(NULL, name);
			}
			free(name);
		}
		workspace_switch(ws, no_auto_back_and_forth);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
