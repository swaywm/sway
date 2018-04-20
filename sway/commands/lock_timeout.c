#include <string.h>
#include <strings.h>
#include <errno.h>
#include "sway/commands.h"

struct cmd_results *cmd_lock_timeout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "lock_timeout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	errno = 0;
	config->lock_timeout = strtol(argv[0], NULL, 10);
	if (errno == EINVAL || errno == ERANGE)
		return cmd_results_new(CMD_INVALID, "lock_timeout", "Invalid lock timeout.");

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
