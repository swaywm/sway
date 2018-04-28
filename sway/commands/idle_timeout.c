#include <string.h>
#include <strings.h>
#include <errno.h>
#include "sway/commands.h"

struct cmd_results *cmd_idle_timeout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "idle_timeout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	errno = 0;
	config->idle_timeout = strtol(argv[0], NULL, 10);
	if (errno == EINVAL || errno == ERANGE)
		return cmd_results_new(CMD_INVALID, "idle_timeout", "Invalid timeout '%s'", argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
