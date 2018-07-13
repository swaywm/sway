#define _XOPEN_SOURCE 500
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "sway/tree/arrange.h"
#include "list.h"

struct cmd_results *cmd_reload(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}

	// store bar ids to check against new bars for barconfig_update events
	list_t *bar_ids = create_list();
	for (int i = 0; i < config->bars->length; ++i) {
		struct bar_config *bar = config->bars->items[i];
		list_add(bar_ids, strdup(bar->id));
	}

	if (!load_main_config(config->current_config_path, true)) {
		return cmd_results_new(CMD_FAILURE, "reload", "Error(s) reloading config.");
	}

	load_swaybars();

	for (int i = 0; i < config->bars->length; ++i) {
		struct bar_config *bar = config->bars->items[i];
		for (int j = 0; j < bar_ids->length; ++j) {
			if (strcmp(bar->id, bar_ids->items[j]) == 0) {
				ipc_event_barconfig_update(bar);
				break;
			}
		}
	}

	for (int i = 0; i < bar_ids->length; ++i) {
		free(bar_ids->items[i]);
	}
	list_free(bar_ids);

	arrange_windows(&root_container);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
