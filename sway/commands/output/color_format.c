#include <strings.h>
#include <wlr/config.h>
#include "sway/commands.h"
#include "sway/config.h"

static bool parse_signal_format(const char *name, enum wlr_output_color_format *format)
{
	if (strcmp(name, "auto") == 0) {
		*format = WLR_OUTPUT_COLOR_FORMAT_UNSPEC;
		return true;
	}
	if (strcmp(name, "rgb") == 0) {
		*format = WLR_OUTPUT_COLOR_FORMAT_RGB;
		return true;
	}
	if (strcmp(name, "ycbcr444") == 0) {
		*format = WLR_OUTPUT_COLOR_FORMAT_YCBCR444;
		return true;
	}
	if (strcmp(name, "ycbcr422") == 0) {
		*format = WLR_OUTPUT_COLOR_FORMAT_YCBCR422;
		return true;
	}
	if (strcmp(name, "ycbcr420") == 0) {
		*format = WLR_OUTPUT_COLOR_FORMAT_YCBCR420;
		return true;
	}
	return false;
}

struct cmd_results *output_cmd_color_format(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	if (argc == 0) {
		return cmd_results_new(CMD_INVALID, "Missing format argument");
	}

	if (!parse_signal_format(argv[0],
				&config->handler_context.output_config->color_format)) {
		return cmd_results_new(CMD_INVALID, "Invalid format");
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
