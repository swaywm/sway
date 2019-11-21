#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <errno.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *input_cmd_xkb_file(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_file", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (strcmp(argv[0], "-") == 0) {
		free(ic->xkb_file);
		ic->xkb_file = NULL;
	} else {
		ic->xkb_file = strdup(argv[0]);
		if (!expand_path(&ic->xkb_file)) {
			error = cmd_results_new(CMD_INVALID, "Invalid syntax (%s)",
					ic->xkb_file);
			free(ic->xkb_file);
			ic->xkb_file = NULL;
			return error;
		}
		if (!ic->xkb_file) {
			sway_log(SWAY_ERROR, "Failed to allocate expanded path");
			return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
		}

		bool can_access = access(ic->xkb_file, F_OK) != -1;
		if (!can_access) {
			sway_log_errno(SWAY_ERROR, "Unable to access xkb file '%s'",
					ic->xkb_file);
			config_add_swaynag_warning("Unable to access xkb file '%s'",
					ic->xkb_file);
		}
	}
	ic->xkb_file_is_set = true;

	sway_log(SWAY_DEBUG, "set-xkb_file for config: %s file: %s",
			ic->identifier, ic->xkb_file);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
