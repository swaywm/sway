#include <string.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

struct cmd_results *output_cmd_scale_filter(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing scale_filter argument.");
	}


	enum scale_filter_mode scale_filter;
	if (strcmp(*argv, "linear") == 0) {
		scale_filter = SCALE_FILTER_LINEAR;
	} else if (strcmp(*argv, "nearest") == 0) {
		scale_filter = SCALE_FILTER_NEAREST;
	} else if (strcmp(*argv, "smart") == 0) {
		scale_filter = SCALE_FILTER_SMART;
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid output scale_filter.");
	}

	struct output_config *oc = config->handler_context.output_config;
	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;

	oc->scale_filter = scale_filter;
	return NULL;
}
