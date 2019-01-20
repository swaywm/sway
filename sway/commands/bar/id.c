#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_id(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "id", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	const char *name = argv[0];
	const char *oldname = config->current_bar->id;
	if (strcmp(name, oldname) == 0) {
		return cmd_results_new(CMD_SUCCESS, NULL);  // NOP
	} else if (strcmp(name, "id") == 0) {
		return cmd_results_new(CMD_INVALID, "id cannot be 'id'");
	}
	// check if id is used by a previously defined bar
	for (int i = 0; i < config->bars->length; ++i) {
		struct bar_config *find = config->bars->items[i];
		if (strcmp(name, find->id) == 0 && config->current_bar != find) {
			return cmd_results_new(CMD_FAILURE,
					"Id '%s' already defined for another bar. Id unchanged (%s).",
					name, oldname);
		}
	}

	sway_log(SWAY_DEBUG, "Renaming bar: '%s' to '%s'", oldname, name);

	// free old bar id
	free(config->current_bar->id);
	config->current_bar->id = strdup(name);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
