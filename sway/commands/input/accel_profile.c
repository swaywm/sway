#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_accel_profile(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "accel_profile", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (strcasecmp(argv[0], "adaptive") == 0) {
		ic->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	} else if (strcasecmp(argv[0], "flat") == 0) {
		ic->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	} else if (strcasecmp(argv[0], "custom") == 0) {
#if HAVE_LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM
		ic->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM;
#else
		return cmd_results_new(CMD_INVALID,
				"Config 'accel_profile custom' not supported (requires libinput >= 1.23.0).");
#endif
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'accel_profile <adaptive|flat|custom>'"
		);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
