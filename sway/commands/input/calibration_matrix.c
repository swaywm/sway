#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *input_cmd_calibration_matrix(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "calibration_matrix", EXPECTED_EQUAL_TO, 6))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	float parsed[6];
	for (int i = 0; i < argc; ++i) {
		char *item = argv[i];
		float x = parse_float(item);
		if (isnan(x)) {
			return cmd_results_new(CMD_FAILURE, "calibration_matrix: unable to parse float: %s", item);
		}
		parsed[i] = x;
	}

	ic->calibration_matrix.configured = true;
	memcpy(ic->calibration_matrix.matrix, parsed, sizeof(ic->calibration_matrix.matrix));

	return cmd_results_new(CMD_SUCCESS, NULL);
}
