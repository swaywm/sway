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

	struct wlr_action_binder_v1_state *state = NULL;
	struct wlr_action_binding_v1 *binding = NULL;
	wl_list_for_each(state, &server.action_binder->states, link) {
		wl_list_for_each(binding, &state->binds, link) {
			wlr_action_binding_v1_trigger(binding, EXT_ACTION_BINDING_V1_TRIGGER_TYPE_ONE_SHOT, 0);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
