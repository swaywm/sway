#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"

struct cmd_results *input_cmd_pointer_accel(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pointer_accel", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	float pointer_accel = parse_float(argv[0]);
	if (isnan(pointer_accel)) {
		return cmd_results_new(CMD_INVALID,
			"Invalid pointer accel; expected float.");
	} if (pointer_accel < -1 || pointer_accel > 1) {
		return cmd_results_new(CMD_INVALID, "Input out of range [-1, 1]");
	}
	ic->pointer_accel = pointer_accel;

	return cmd_results_new(CMD_SUCCESS, NULL);
}

#if HAVE_LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define ACCEL_CUSTOM_MAX (1 + ARRAY_SIZE(((struct input_config *)0)->pointer_accel_custom.points))
struct cmd_results *input_cmd_pointer_accel_custom(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pointer_accel_custom", EXPECTED_AT_LEAST, 3))) {
		return error;
	}
	if ((error = checkarg(argc, "pointer_accel_custom", EXPECTED_AT_MOST, ACCEL_CUSTOM_MAX))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	for(int i=0; i<argc; ++i) {
		double dbl = parse_float(argv[i]);
		if (isnan(dbl) || dbl < 0.0) {
			return cmd_results_new(CMD_INVALID,
				"Invalid pointer_accel_custom value; expected non-negative double.");
		}
		if (i == 0) {
			ic->pointer_accel_custom.step = dbl;
		} else {
			ic->pointer_accel_custom.points[i-1] = dbl;
		}
	}
	ic->pointer_accel_custom.npoints = argc - 1;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
#endif
