#include "sway/commands.h"
#include "sway/config.h"
#include <errno.h>

struct cmd_results *cmd_force_display_urgency_hint(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "force_display_urgency_hint",
					EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	errno = 0;
	char *end;
	int timeout = (int)strtol(argv[0], &end, 10);
	if (errno || end == argv[0] || (*end && strcmp(end, "ms") != 0)) {
		return cmd_results_new(CMD_INVALID, "timeout integer invalid");
	}

	if (argc > 1 && strcmp(argv[1], "ms") != 0) {
		return cmd_results_new(CMD_INVALID,
				"Expected 'force_display_urgency_hint <timeout> [ms]'");
	}

	config->urgent_timeout = timeout > 0 ? timeout : 0;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
