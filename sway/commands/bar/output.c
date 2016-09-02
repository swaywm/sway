#include <string.h>
#include "commands.h"
#include "list.h"
#include "log.h"

struct cmd_results *bar_cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "output", "No bar defined.");
	}

	const char *output = argv[0];
	list_t *outputs = config->current_bar->outputs;
	if (!outputs) {
		outputs = create_list();
		config->current_bar->outputs = outputs;
	}

	int i;
	int add_output = 1;
	if (strcmp("*", output) == 0) {
		// remove all previous defined outputs and replace with '*'
		for (i = 0; i < outputs->length; ++i) {
			free(outputs->items[i]);
			list_del(outputs, i);
		}
	} else {
		// only add output if not already defined with either the same
		// name or as '*'
		for (i = 0; i < outputs->length; ++i) {
			const char *find = outputs->items[i];
			if (strcmp("*", find) == 0 || strcmp(output, find) == 0) {
				add_output = 0;
				break;
			}
		}
	}

	if (add_output) {
		list_add(outputs, strdup(output));
		sway_log(L_DEBUG, "Adding bar: '%s' to output '%s'", config->current_bar->id, output);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
