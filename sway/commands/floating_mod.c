#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input_state.h"
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *cmd_floating_mod(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_modifier", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	list_t *split = split_string(argv[0], "+");
	config->floating_mod = 0;

	// set modifier keys
	for (size_t i = 0; i < split->length; ++i) {
		config->floating_mod |= get_modifier_mask_by_name(*(char **)list_get(split, i));
	}
	free_flat_list(split);
	if (!config->floating_mod) {
		error = cmd_results_new(CMD_INVALID, "floating_modifier", "Unknown keys %s", argv[0]);
		return error;
	}

	if (argc >= 2) {
		if (strcasecmp("inverse", argv[1]) == 0) {
			config->dragging_key = M_RIGHT_CLICK;
			config->resizing_key = M_LEFT_CLICK;
		} else if (strcasecmp("normal", argv[1]) == 0) {
			config->dragging_key = M_LEFT_CLICK;
			config->resizing_key = M_RIGHT_CLICK;
		} else {
			error = cmd_results_new(CMD_INVALID, "floating_modifier", "Invalid definition %s", argv[1]);
			return error;
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
