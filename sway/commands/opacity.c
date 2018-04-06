#include <assert.h>
#include <stdlib.h>
#include "sway/commands.h"
#include "sway/tree/view.h"
#include "log.h"

static bool parse_opacity(const char *opacity, float *val) {
	char *err;
	*val = strtof(opacity, &err);
	if (*val < 0 || *val > 1 || *err) {
		return false;
	}
	return true;
}

struct cmd_results *cmd_opacity(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	struct sway_container *con =
		config->handler_context.current_container;

	float opacity = 0.0f;

	if (!parse_opacity(argv[0], &opacity)) {
		return cmd_results_new(CMD_INVALID, "opacity <value>",
				"Invalid value (expected 0..1): %s", argv[0]);
	}

	con->alpha = opacity;
	container_damage_whole(con);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
