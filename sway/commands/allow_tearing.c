#include <sway/commands.h>
#include "sway/config.h"
#include "sway/tree/view.h"
#include "util.h"

struct cmd_results *cmd_allow_tearing(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "allow_tearing", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct sway_container *container = config->handler_context.container;
	if (!container || !container->view) {
		return cmd_results_new(CMD_INVALID, "Tearing can only be allowed on views");
	}

	bool wants_tearing = parse_boolean(argv[0], true);

	struct sway_view *view = container->view;
	view->tearing_mode = wants_tearing ? TEARING_OVERRIDE_TRUE :
		TEARING_OVERRIDE_FALSE;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
