#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"

static bool parse_coords(const char *str, double *x, double *y, bool *mm) {
	*mm = false;

	char *end;
	*x = strtod(str, &end);
	if (end[0] != 'x') {
		return false;
	}
	++end;

	*y = strtod(end, &end);
	if (end[0] == 'm') {
		// Expect mm
		if (end[1] != 'm') {
			return false;
		}
		*mm = true;
		end = &end[2];
	}
	if (end[0] != '\0') {
		return false;
	}

	return true;
}

struct cmd_results *input_cmd_map_from_region(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "map_from_region", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined");
	}

	ic->mapped_from_region =
		calloc(1, sizeof(struct input_config_mapped_from_region));

	bool mm1, mm2;
	if (!parse_coords(argv[0], &ic->mapped_from_region->x1,
			&ic->mapped_from_region->y1, &mm1)) {
		free(ic->mapped_from_region);
		ic->mapped_from_region = NULL;
		return cmd_results_new(CMD_FAILURE, "Invalid top-left coordinates");
	}
	if (!parse_coords(argv[1], &ic->mapped_from_region->x2,
			&ic->mapped_from_region->y2, &mm2)) {
		free(ic->mapped_from_region);
		ic->mapped_from_region = NULL;
		return cmd_results_new(CMD_FAILURE, "Invalid bottom-right coordinates");
	}
	if (ic->mapped_from_region->x1 > ic->mapped_from_region->x2 ||
			ic->mapped_from_region->y1 > ic->mapped_from_region->y2) {
		free(ic->mapped_from_region);
		ic->mapped_from_region = NULL;
		return cmd_results_new(CMD_FAILURE, "Invalid rectangle");
	}
	if (mm1 != mm2) {
		free(ic->mapped_from_region);
		ic->mapped_from_region = NULL;
		return cmd_results_new(CMD_FAILURE,
			"Both coordinates must be in the same unit");
	}
	ic->mapped_from_region->mm = mm1;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
