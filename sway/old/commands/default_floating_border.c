#include <errno.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/container.h"

struct cmd_results *cmd_default_floating_border(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "default_floating_border", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (argc > 2) {
		return cmd_results_new(CMD_INVALID, "default_floating_border",
			"Expected 'default_floating_border <normal|none|pixel> [<n>]");
	}

	enum swayc_border_types border = config->floating_border;
	int thickness = config->floating_border_thickness;

	if (strcasecmp(argv[0], "none") == 0) {
		border = B_NONE;
	} else if (strcasecmp(argv[0], "normal") == 0) {
		border = B_NORMAL;
	} else if (strcasecmp(argv[0], "pixel") == 0) {
		border = B_PIXEL;
	} else {
		return cmd_results_new(CMD_INVALID, "default_floating_border",
			"Expected 'default_floating_border <normal|none|pixel> [<n>]");
	}

	if (argc == 2 && (border == B_NORMAL || border == B_PIXEL)) {
		thickness = (int)strtol(argv[1], NULL, 10);
		if (errno == ERANGE || thickness < 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "default_floating_border",
				"Number is out out of range.");
		}
	}

	config->floating_border = border;
	config->floating_border_thickness = thickness;

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
