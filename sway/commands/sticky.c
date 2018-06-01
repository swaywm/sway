#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "list.h"

struct cmd_results *cmd_sticky(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "sticky", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct sway_container *container =
		config->handler_context.current_container;
	if (!container_is_floating(container)) {
		return cmd_results_new(CMD_FAILURE, "sticky",
			"Can't set sticky on a tiled container");
	}

	bool wants_sticky;
	if (strcasecmp(argv[0], "enable") == 0) {
		wants_sticky = true;
	} else if (strcasecmp(argv[0], "disable") == 0) {
		wants_sticky = false;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		wants_sticky = !container->is_sticky;
	} else {
		return cmd_results_new(CMD_FAILURE, "sticky",
			"Expected 'sticky <enable|disable|toggle>'");
	}

	container->is_sticky = wants_sticky;

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
