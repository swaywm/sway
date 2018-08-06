#define _XOPEN_SOURCE 500
#include <string.h>
#include <strings.h>
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"

static const char* expected_syntax =
	"Expected 'rename workspace <old_name> to <new_name>' or "
	"'rename workspace to <new_name>'";

struct cmd_results *cmd_rename(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "rename", EXPECTED_AT_LEAST, 3))) {
		return error;
	}
	if (strcasecmp(argv[0], "workspace") != 0) {
		return cmd_results_new(CMD_INVALID, "rename", expected_syntax);
	}

	int argn = 1;
	struct sway_container *workspace;

	if (strcasecmp(argv[1], "to") == 0) {
		// 'rename workspace to new_name'
		workspace = config->handler_context.current_container;
		if (workspace->type != C_WORKSPACE) {
			workspace = container_parent(workspace, C_WORKSPACE);
		}
	} else if (strcasecmp(argv[1], "number") == 0) {
		// 'rename workspace number x to new_name'
		workspace = workspace_by_number(argv[2]);
		while (argn < argc && strcasecmp(argv[argn], "to") != 0) {
			++argn;
		}
	} else {
		// 'rename workspace old_name to new_name'
		int end = argn;
		while (end < argc && strcasecmp(argv[end], "to") != 0) {
			++end;
		}
		char *old_name = join_args(argv + argn, end - argn);
		workspace = workspace_by_name(old_name);
		free(old_name);
		argn = end;
	}

	if (!workspace) {
		return cmd_results_new(CMD_INVALID, "rename",
				"There is no workspace with that name");
	}

	++argn; // move past "to"

	if (argn >= argc) {
		return cmd_results_new(CMD_INVALID, "rename", expected_syntax);
	}

	char *new_name = join_args(argv + argn, argc - argn);
	if (strcasecmp(new_name, "next") == 0 ||
			strcasecmp(new_name, "prev") == 0 ||
			strcasecmp(new_name, "next_on_output") == 0 ||
			strcasecmp(new_name, "prev_on_output") == 0 ||
			strcasecmp(new_name, "back_and_forth") == 0 ||
			strcasecmp(new_name, "current") == 0) {
		free(new_name);
		return cmd_results_new(CMD_INVALID, "rename",
				"Cannot use special workspace name '%s'", argv[argn]);
	}
	struct sway_container *tmp_workspace = workspace_by_name(new_name);
	if (tmp_workspace) {
		free(new_name);
		return cmd_results_new(CMD_INVALID, "rename",
				"Workspace already exists");
	}

	wlr_log(WLR_DEBUG, "renaming workspace '%s' to '%s'", workspace->name, new_name);
	free(workspace->name);
	workspace->name = new_name;

	container_sort_workspaces(workspace->parent);
	ipc_event_workspace(NULL, workspace, "rename");

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
