#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_color_format(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing color format argument.");
	}

	if (strcmp(*argv, "auto") == 0) {
		config->handler_context.output_config->color_format =
			WLR_OUTPUT_COLOR_FORMAT_AUTO;
	} else if (strcmp(*argv, "rgb") == 0) {
		config->handler_context.output_config->color_format =
			WLR_OUTPUT_COLOR_FORMAT_RGB444;
	} else if (strcmp(*argv, "yuv444") == 0) {
		config->handler_context.output_config->color_format =
			WLR_OUTPUT_COLOR_FORMAT_YCBCR444;
	} else if (strcmp(*argv, "yuv422") == 0) {
		config->handler_context.output_config->color_format =
			WLR_OUTPUT_COLOR_FORMAT_YCBCR422;
	} else if (strcmp(*argv, "yuv420") == 0) {
		config->handler_context.output_config->color_format =
			WLR_OUTPUT_COLOR_FORMAT_YCBCR420;
	} else {
		return cmd_results_new(CMD_INVALID,
			"Invalid color format. Must be a value in (auto|rgb|yuv444|yuv422|yuv420).");
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
