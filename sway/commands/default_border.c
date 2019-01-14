#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/container.h"

struct cmd_results *cmd_default_border(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "default_border", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcmp(argv[0], "none") == 0) {
		config->border = B_NONE;
	} else if (strcmp(argv[0], "normal") == 0) {
		config->border = B_NORMAL;
	} else if (strcmp(argv[0], "pixel") == 0) {
		config->border = B_PIXEL;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'default_border <none|normal|pixel>' or 'default_border <normal|pixel> <px>'");
	}
	if (argc == 2) {
		config->border_thickness = atoi(argv[1]);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
