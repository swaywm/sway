#include <stdlib.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_tray_padding(int argc, char **argv) {
	const char *cmd_name = "tray_padding";
#ifndef ENABLE_TRAY
	return cmd_results_new(CMD_INVALID, cmd_name, "Invalid %s command"
			"%s called, but sway was compiled without tray support",
			cmd_name, cmd_name);
#else
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, cmd_name, "No bar defined.");
	}

	if (argc == 1 || (argc == 2 && strcasecmp("px", argv[1]) == 0)) {
		char *inv;
		uint32_t padding = strtoul(argv[0], &inv, 10);
		if (*inv == '\0' || strcasecmp(inv, "px") == 0) {
			config->current_bar->tray_padding = padding;
			sway_log(L_DEBUG, "Enabling tray padding of %d px on bar: %s", padding, config->current_bar->id);
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}
	}
	return cmd_results_new(CMD_FAILURE, cmd_name,
		"Expected 'tray_padding <padding>[px]'");
#endif
}
