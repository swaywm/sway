#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"
#include "util.h"

struct cmd_results *bar_cmd_tray_last(int argc, char **argv) {
#if HAVE_TRAY
	struct cmd_results *error = NULL;
        sway_log(SWAY_DEBUG, "Checking tray_last command");
	if ((error = checkarg(argc,
				"tray_last", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->current_bar->tray_last =
		parse_boolean(argv[0], config->current_bar->tray_last);

	if (config->current_bar->tray_last) {
		config->current_bar->tray_last = true;

		sway_log(SWAY_DEBUG, "Making tray the last item on the bar: %s",
				config->current_bar->id);
	} else {
		sway_log(SWAY_DEBUG, "Making tray the before last item on the bar: %s",
				config->current_bar->id);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
#else
	return cmd_results_new(CMD_INVALID,
			"Sway has been compiled without tray support");
#endif
}
