#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "config.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"

struct cmd_results *bar_cmd_tray_output(int argc, char **argv) {
#if HAVE_TRAY
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tray_output", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "tray_output", "No bar defined.");
	}

	list_t *outputs = config->current_bar->tray_outputs;
	if (!outputs) {
		config->current_bar->tray_outputs = outputs = create_list();
	}

	if (strcmp(argv[0], "none") == 0) {
		wlr_log(WLR_DEBUG, "Hiding tray on bar: %s", config->current_bar->id);
		for (int i = 0; i < outputs->length; ++i) {
			free(outputs->items[i]);
		}
		outputs->length = 0;
	} else {
		wlr_log(WLR_DEBUG, "Showing tray on output '%s' for bar: %s", argv[0],
				config->current_bar->id);
	}
	list_add(outputs, strdup(argv[0]));

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
#else
	return cmd_results_new(CMD_INVALID, "tray_output",
			"Sway has been compiled without tray support");
#endif
}
