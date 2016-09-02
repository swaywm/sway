#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *cmd_floating_maximum_size(int argc, char **argv) {
	struct cmd_results *error = NULL;
	int32_t width;
	int32_t height;
	char *ptr;

	if ((error = checkarg(argc, "floating_maximum_size", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}
	width = strtol(argv[0], &ptr, 10);
	height = strtol(argv[2], &ptr, 10);

	if (width < -1) {
		sway_log(L_DEBUG, "floating_maximum_size invalid width value: '%s'", argv[0]);

	} else {
		config->floating_maximum_width = width;

	}

	if (height < -1) {
		sway_log(L_DEBUG, "floating_maximum_size invalid height value: '%s'", argv[2]);
	}
	else {
		config->floating_maximum_height = height;

	}

	sway_log(L_DEBUG, "New floating_maximum_size: '%d' x '%d'", config->floating_maximum_width,
		config->floating_maximum_height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
