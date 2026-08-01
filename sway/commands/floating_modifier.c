#include "strings.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/keyboard.h"

struct cmd_results *cmd_floating_modifier(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_modifier", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	
	if (strcasecmp(argv[0], "none") == 0) {
		config->floating_mod = 0;
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	
	if (argc == 1 || strcasecmp(argv[1], "normal") == 0) {
		config->floating_mod_inverse = false;
	} else if (strcasecmp(argv[1], "inverse") == 0) {
		config->floating_mod_inverse = true;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Usage: floating_modifier <mod> [inverse|normal]");
	}

	uint32_t mod = 0;
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
				"Invalid modifier '%s'", (char *)split->items[i]);
			list_free_items_and_destroy(split);
			return error;
		}
	}
	list_free_items_and_destroy(split);
	
	config->floating_mod = mod;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
