#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_transform(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "output", "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing transform argument.");
	}

	struct output_config *output = config->handler_context.output_config;
	if (strcmp(*argv, "normal") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_NORMAL;
	} else if (strcmp(*argv, "90") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_90;
	} else if (strcmp(*argv, "180") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_180;
	} else if (strcmp(*argv, "270") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_270;
	} else if (strcmp(*argv, "flipped") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_FLIPPED;
	} else if (strcmp(*argv, "flipped-90") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_FLIPPED_90;
	} else if (strcmp(*argv, "flipped-180") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_FLIPPED_180;
	} else if (strcmp(*argv, "flipped-270") == 0) {
		output->transform = WL_OUTPUT_TRANSFORM_FLIPPED_270;
	} else {
		return cmd_results_new(CMD_INVALID, "output",
			"Invalid output transform.");
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
