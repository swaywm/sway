#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

struct cmd_results *output_cmd_toggle(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	struct output_config *oc = config->handler_context.output_config;

	if (strcmp(oc->name, "*") == 0) {
		return cmd_results_new(CMD_INVALID,
				"Cannot apply toggle to all outputs.");
	}

	struct sway_output *sway_output = all_output_by_name_or_id(oc->name);

	if (sway_output == NULL) {
		return cmd_results_new(CMD_FAILURE,
				"Cannot apply toggle to unknown output %s", oc->name);
	}

	oc = find_output_config(sway_output);

	if (!oc || oc->enabled != 0) {
		config->handler_context.output_config->enabled = 0;
	} else {
		config->handler_context.output_config->enabled = 1;
	}

	free(oc);
	config->handler_context.leftovers.argc = argc;
	config->handler_context.leftovers.argv = argv;
	return NULL;
}

