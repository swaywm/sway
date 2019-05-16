#include <string.h>
#include "sway/commands.h"
#include "sway/input/keyboard.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *bar_cmd_modifier(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "modifier", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	uint32_t mod = 0;
	if (strcmp(argv[0], "none") != 0) {
		list_t *split = split_string(argv[0], "+");
		for (int i = 0; i < split->length; ++i) {
			uint32_t tmp_mod;
			if ((tmp_mod = get_modifier_mask_by_name(split->items[i])) > 0) {
				mod |= tmp_mod;
			} else if (strcmp(split->items[i], "none") == 0) {
				error = cmd_results_new(CMD_INVALID,
						"none cannot be used along with other modifiers");
				list_free_items_and_destroy(split);
				return error;
			} else {
				error = cmd_results_new(CMD_INVALID,
					"Unknown modifier '%s'", (char *)split->items[i]);
				list_free_items_and_destroy(split);
				return error;
			}
		}
		list_free_items_and_destroy(split);
	}
	config->current_bar->modifier = mod;
	sway_log(SWAY_DEBUG,
			"Show/Hide the bar when pressing '%s' in hide mode.", argv[0]);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
