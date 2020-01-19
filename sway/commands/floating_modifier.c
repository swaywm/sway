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

	uint32_t mod = get_modifier_mask_by_name(argv[0]);
	if (!mod) {
		return cmd_results_new(CMD_INVALID, "Invalid modifier");
	}

	if (argc == 1 || strcasecmp(argv[1], "normal") == 0) {
		config->floating_mod_inverse = false;
	} else if (strcasecmp(argv[1], "inverse") == 0) {
		config->floating_mod_inverse = true;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Usage: floating_modifier <mod> [inverse|normal]");
	}

	config->floating_mod = mod;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
