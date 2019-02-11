#include <string.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

struct cmd_results *output_cmd_subpixel(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing subpixel argument.");
	}
	enum wl_output_subpixel subpixel;

	if (strcmp(*argv, "rgb") == 0) {
		subpixel = WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
	} else if (strcmp(*argv, "bgr") == 0) {
		subpixel = WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
	} else if (strcmp(*argv, "vrgb") == 0) {
		subpixel = WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
	} else if (strcmp(*argv, "vbgr") == 0) {
		subpixel = WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
	} else if (strcmp(*argv, "none") == 0) {
		subpixel = WL_OUTPUT_SUBPIXEL_NONE;
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid output subpixel.");
	}

	struct output_config *oc = config->handler_context.output_config;
	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;

	oc->subpixel = subpixel;
	return NULL;
}
