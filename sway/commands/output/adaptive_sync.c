#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "util.h"

struct cmd_results *output_cmd_adaptive_sync(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (argc == 0) {
		return cmd_results_new(CMD_INVALID, "Missing adaptive_sync argument");
	}

	bool current_value = true;
	if (strcasecmp(argv[0], "toggle") == 0) {
		const char *oc_name = config->handler_context.output_config->name;
		if (strcmp(oc_name, "*") == 0) {
			return cmd_results_new(CMD_INVALID,
				"Cannot apply toggle to all outputs");
		}

		struct sway_output *sway_output = all_output_by_name_or_id(oc_name);
		if (!sway_output || !sway_output->wlr_output) {
			return cmd_results_new(CMD_FAILURE,
				"Cannot apply toggle to unknown output %s", oc_name);
		}

		current_value =
			sway_output->wlr_output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED;
	}

	config->handler_context.output_config->adaptive_sync = parse_boolean(argv[0], current_value);

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
