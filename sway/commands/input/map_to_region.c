#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *input_cmd_map_to_region(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "map_to_region", EXPECTED_EQUAL_TO, 4))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined");
	}

	ic->mapped_to = MAPPED_TO_REGION;
	ic->mapped_to_region = calloc(1, sizeof(struct wlr_box));

	const char *errstr;
	char *end;

	ic->mapped_to_region->x = strtol(argv[0], &end, 10);
	if (end[0] != '\0') {
		errstr = "Invalid X coordinate";
		goto error;
	}

	ic->mapped_to_region->y = strtol(argv[1], &end, 10);
	if (end[0] != '\0') {
		errstr = "Invalid Y coordinate";
		goto error;
	}

	ic->mapped_to_region->width = strtol(argv[2], &end, 10);
	if (end[0] != '\0' || ic->mapped_to_region->width < 1) {
		errstr = "Invalid width";
		goto error;
	}

	ic->mapped_to_region->height = strtol(argv[3], &end, 10);
	if (end[0] != '\0' || ic->mapped_to_region->height < 1) {
		errstr = "Invalid height";
		goto error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);

error:
	free(ic->mapped_to_region);
	ic->mapped_to_region = NULL;
	return cmd_results_new(CMD_FAILURE, errstr);
}
