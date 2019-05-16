#include <stdlib.h>
#include <strings.h>
#include "config.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"

struct cmd_results *bar_cmd_tray_padding(int argc, char **argv) {
#if HAVE_TRAY
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tray_padding", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "tray_padding", EXPECTED_AT_MOST, 2))) {
		return error;
	}

	struct bar_config *bar = config->current_bar;

	char *end;
	int padding = strtol(argv[0], &end, 10);
	if (padding < 0 || (*end != '\0' && strcasecmp(end, "px") != 0)) {
		return cmd_results_new(CMD_INVALID,
				"[Bar %s] Invalid tray padding value: %s", bar->id, argv[0]);
	}

	if (argc == 2 && strcasecmp(argv[1], "px") != 0) {
		return cmd_results_new(CMD_INVALID,
				"Expected 'tray_padding <px> [px]'");
	}

	sway_log(SWAY_DEBUG, "[Bar %s] Setting tray padding to %d", bar->id, padding);
	config->current_bar->tray_padding = padding;
	return cmd_results_new(CMD_SUCCESS, NULL);
#else
	return cmd_results_new(CMD_INVALID,
			"Sway has been compiled without tray support");
#endif
}
