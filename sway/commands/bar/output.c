#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <string.h>
#include "sway/commands.h"
#include "list.h"
#include "log.h"

struct cmd_results *bar_cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	const char *output = argv[0];
	list_t *outputs = config->current_bar->outputs;
	if (!outputs) {
		outputs = create_list();
		config->current_bar->outputs = outputs;
	}

	bool add_output = true;
	if (strcmp("*", output) == 0) {
		// remove all previous defined outputs and replace with '*'
		while (outputs->length) {
			free(outputs->items[0]);
			list_del(outputs, 0);
		}
	} else {
		// only add output if not already defined, if the list has '*', remove
		// it, in favor of a manual list
		for (int i = 0; i < outputs->length; ++i) {
			const char *find = outputs->items[i];
			if (strcmp("*", find) == 0) {
				free(outputs->items[i]);
				list_del(outputs, i);
			} else if (strcmp(output, find) == 0) {
				add_output = false;
				break;
			}
		}
	}

	if (add_output) {
		list_add(outputs, strdup(output));
		sway_log(SWAY_DEBUG, "Adding bar: '%s' to output '%s'",
				config->current_bar->id, output);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
