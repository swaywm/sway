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

	struct sway_container *current_container = config->handler_context.current_container;
	struct sway_container *old_workspace = NULL, *old_output = NULL;
	if (current_container) {
		if (current_container->type == C_WORKSPACE) {
			old_workspace = current_container;
		} else {
			old_workspace = container_parent(current_container, C_WORKSPACE);
		}
		old_output = container_parent(current_container, C_OUTPUT);
	}

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
		wlr_log(L_DEBUG, "Assigning workspace %s to output %s", wso->workspace, wso->output);
		list_add(config->workspace_outputs, wso);
	} else {
		if (config->reading || !config->active) {
			return cmd_results_new(CMD_DEFER, "workspace", NULL);
		}
		struct sway_container *ws = NULL;
		if (strcasecmp(argv[0], "number") == 0) {
			if (!(ws = workspace_by_number(argv[1]))) {
				char *name = join_args(argv + 1, argc - 1);
				ws = container_workspace_create(NULL, name);
				free(name);
			}
		} else if (strcasecmp(argv[0], "next") == 0) {
			ws = workspace_next(old_workspace);
		} else if (strcasecmp(argv[0], "prev") == 0) {
			ws = workspace_prev(old_workspace);
		} else if (strcasecmp(argv[0], "next_on_output") == 0) {
			ws = workspace_output_next(old_output);
		} else if (strcasecmp(argv[0], "prev_on_output") == 0) {
			ws = workspace_output_prev(old_output);
		} else if (strcasecmp(argv[0], "back_and_forth") == 0) {
			// if auto_back_and_forth is enabled, workspace_switch will swap
			// the workspaces. If we created prev_workspace here, workspace_switch
			// would put us back on original workspace.
			if (config->auto_back_and_forth) {
				ws = old_workspace;
			} else if (prev_workspace_name
					&& !(ws = workspace_by_name(prev_workspace_name))) {
				ws = container_workspace_create(NULL, prev_workspace_name);
			}
		} else {
			char *name = join_args(argv, argc);
			if (!(ws = workspace_by_name(name))) {
				ws = container_workspace_create(NULL, name);
			}
			free(name);
		}
		workspace_switch(ws);
		current_container =
			seat_get_focus(config->handler_context.seat);
		struct sway_container *new_output = container_parent(current_container, C_OUTPUT);

		if (config->mouse_warping && old_output != new_output) {
			// TODO: Warp mouse
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
