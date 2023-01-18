#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "util.h"

struct cmd_results *cmd_shadow_color(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "shadow_color", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	uint32_t color;
	if (!parse_color(argv[0], &color)) {
		return cmd_results_new(CMD_INVALID, "Invalid %s color %s",
				"shadow_color", argv[0]);
	}
	color_to_rgba(config->shadow_color, color);

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
