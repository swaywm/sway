#include "sway/config.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/server.h"
#include "util.h"

struct cmd_results *cmd_xwayland(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xwayland", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

#ifdef HAVE_XWAYLAND
	enum xwayland_mode xwayland;
	if (strcmp(argv[0], "force") == 0) {
		xwayland = XWAYLAND_MODE_IMMEDIATE;
	} else if (parse_boolean(argv[0], true)) {
		xwayland = XWAYLAND_MODE_LAZY;
	} else {
		xwayland = XWAYLAND_MODE_DISABLED;
	}

	if (config->reloading && config->xwayland != xwayland) {
		return cmd_results_new(CMD_FAILURE,
				"xwayland can only be enabled/disabled at launch");
	}
	config->xwayland = xwayland;
#else
	sway_log(SWAY_INFO, "Ignoring `xwayland` command, "
		"sway hasn't been built with Xwayland support");
#endif

	return cmd_results_new(CMD_SUCCESS, NULL);
}
