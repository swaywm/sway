#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "log.h"

static struct cmd_results *bar_set_mode(struct bar_config *bar, const char *mode) {
	char *old_mode = bar->mode;
	if (strcasecmp("toggle", mode) == 0 && !config->reading) {
		if (strcasecmp("dock", bar->mode) == 0) {
			bar->mode = strdup("hide");
		} else if (strcasecmp("hide", bar->mode) == 0) {
			bar->mode = strdup("dock");
		}
	} else if (strcasecmp("dock", mode) == 0) {
		bar->mode = strdup("dock");
	} else if (strcasecmp("hide", mode) == 0) {
		bar->mode = strdup("hide");
	} else if (strcasecmp("invisible", mode) == 0) {
		bar->mode = strdup("invisible");
	} else {
		return cmd_results_new(CMD_INVALID, "mode", "Invalid value %s", mode);
	}

	if (strcmp(old_mode, bar->mode) != 0) {
		if (!config->reading) {
			ipc_event_barconfig_update(bar);

			// active bar modifiers might have changed.
			update_active_bar_modifiers();
		}
		sway_log(L_DEBUG, "Setting mode: '%s' for bar: %s", bar->mode, bar->id);
	}

	// free old mode
	free(old_mode);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "mode", EXPECTED_LESS_THAN, 3))) {
		return error;
	}

	if (config->reading && argc > 1) {
		return cmd_results_new(CMD_INVALID, "mode", "Unexpected value %s in config mode", argv[1]);
	}

	const char *mode = argv[0];

	if (config->reading) {
		return bar_set_mode(config->current_bar, mode);
	}

	const char *id = NULL;
	if (argc == 2) {
		id = argv[1];
	}

	int i;
	struct bar_config *bar;
	for (i = 0; i < config->bars->length; ++i) {
		bar = config->bars->items[i];
		if (id && strcmp(id, bar->id) == 0) {
			return bar_set_mode(bar, mode);
		}

		error = bar_set_mode(bar, mode);
		if (error) {
			return error;
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
