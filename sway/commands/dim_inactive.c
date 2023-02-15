#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "sway/output.h"
#include "sway/tree/container.h"

struct cmd_results *cmd_dim_inactive(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "dim_inactive", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *err;
	float val = strtof(argv[0], &err);
	if (*err || val < 0.0f || val > 1.0f) {
		return cmd_results_new(CMD_INVALID, "dim_inactive float invalid");
	}

	struct sway_container *container = config->handler_context.container;
    if (!container) {
        return cmd_results_new(CMD_INVALID, "cmd_dim cannot be used without a for_window rule");
    }

    container->dim = val;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
