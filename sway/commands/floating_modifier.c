#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *cmd_floating_modifier(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_modifier", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	uint32_t mod = get_modifier_mask_by_name(argv[0]);
	if (!mod) {
		return cmd_results_new(CMD_INVALID, "floating_modifier",
				"Invalid modifier");
	}

	config->floating_mod = mod;

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
