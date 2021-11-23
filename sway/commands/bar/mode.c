#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "log.h"

static struct cmd_results *bar_set_mode(struct bar_config *bar, const char *mode) {
	char *old_mode = bar->mode;
	if (strcasecmp("toggle", mode) == 0 && !config->reading) {
		if (strcasecmp("dock", bar->mode) == 0) {
			bar->mode = strdup("hide");
		} else{
			bar->mode = strdup("dock");
		}
	} else if (strcasecmp("dock", mode) == 0) {
		bar->mode = strdup("dock");
	} else if (strcasecmp("hide", mode) == 0) {
		bar->mode = strdup("hide");
	} else if (strcasecmp("invisible", mode) == 0) {
		bar->mode = strdup("invisible");
	} else if (strcasecmp("overlay", mode) == 0) {
		bar->mode = strdup("overlay");
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid value %s", mode);
	}

	if (strcmp(old_mode, bar->mode) != 0) {
		if (!config->current_bar) {
			ipc_event_barconfig_update(bar);
		}
		sway_log(SWAY_DEBUG, "Setting mode: '%s' for bar: %s", bar->mode, bar->id);
	}

	// free old mode
	free(old_mode);
	return NULL;
}

struct cmd_results *bar_cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "mode", EXPECTED_AT_MOST, 2))) {
		return error;
	}
	if (config->reading && argc > 1) {
		return cmd_results_new(CMD_INVALID,
				"Unexpected value %s in config mode", argv[1]);
	}

	if (config->current_bar && argc == 2 &&
			strcmp(config->current_bar->id, argv[1]) != 0) {
		return cmd_results_new(CMD_INVALID, "Conflicting bar ids: %s and %s",
				config->current_bar->id, argv[1]);
	}

	const char *mode = argv[0];
	if (config->current_bar) {
		error = bar_set_mode(config->current_bar, mode);
	} else {
		const char *id = argc == 2 ? argv[1] : NULL;
		for (int i = 0; i < config->bars->length; ++i) {
			struct bar_config *bar = config->bars->items[i];
			if (id) {
				if (strcmp(id, bar->id) == 0) {
					error = bar_set_mode(bar, mode);
					break;
				}
			} else if ((error = bar_set_mode(bar, mode))) {
				break;
			}
		}
	}
	return error ? error : cmd_results_new(CMD_SUCCESS, NULL);
}
