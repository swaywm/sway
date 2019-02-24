#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_include(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "include", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	// We don't care if the included config(s) fails to load.
	load_include_configs(argv[0], config, &config->swaynag_config_errors);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
