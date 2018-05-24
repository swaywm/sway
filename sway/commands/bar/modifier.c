#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *bar_cmd_modifier(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "modifier", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "modifier", "No bar defined.");
	}

	uint32_t mod = 0;
	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		uint32_t tmp_mod;
		if ((tmp_mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			mod |= tmp_mod;
			continue;
		} else {
			free_flat_list(split);
			return cmd_results_new(CMD_INVALID, "modifier",
					"Unknown modifier '%s'", split->items[i]);
		}
	}
	free_flat_list(split);
	config->current_bar->modifier = mod;
	sway_log(L_DEBUG,
			"Show/Hide the bar when pressing '%s' in hide mode.", argv[0]);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
