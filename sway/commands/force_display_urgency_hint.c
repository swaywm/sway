#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_force_display_urgency_hint(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "force_display_urgency_hint",
					EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *err;
	int timeout = (int)strtol(argv[0], &err, 10);
	if (*err) {
		if (strcmp(err, "ms") != 0) {
			return cmd_results_new(CMD_INVALID, "force_display_urgency_hint",
					"Expected 'force_display_urgency_hint <timeout> ms'");
		}
	}

	config->urgent_timeout = timeout > 0 ? timeout : 0;

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
