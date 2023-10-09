#include "log.h"
#include <wlr/types/wlr_action_binder_v1.h>
#include "ext-action-binder-v1-protocol.h"
#include "sway/commands.h"
#include "sway/server.h"
#include "sway/input/input-manager.h"

struct cmd_results *cmd_action(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "action", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	wlr_action_binder_v1_trigger(server.action_binder, argv[0], argv[1],
			EXT_ACTION_BINDING_V1_TRIGGER_TYPE_ONE_SHOT);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
