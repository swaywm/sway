#include <strings.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "util.h"

// workspace_namespace [global|output]
struct cmd_results *cmd_workspace_namespace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_namespace", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "global") == 0 ||
			strcasecmp(argv[0], "default") == 0) {
		config->workspace_namespace = WORKSPACE_NAMESPACE_GLOBAL;
	} else if (strcasecmp(argv[0], "output") == 0) {
		config->workspace_namespace = WORKSPACE_NAMESPACE_OUTPUT;
	} else {
		return cmd_results_new(CMD_FAILURE,
				"Expected 'workspace_namespace <default|global|output>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}