#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"

struct cmd_results *input_cmd_rotation_angle(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "rotation_angle", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	float rotation_angle = parse_float(argv[0]);
	if (isnan(rotation_angle)) {
		return cmd_results_new(CMD_INVALID,
			"Invalid rotation_angle; expected float.");
	} if (rotation_angle < 0 || rotation_angle > 360) {
		return cmd_results_new(CMD_INVALID, "Input out of range [0, 360)");
	}
	ic->rotation_angle = rotation_angle;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
