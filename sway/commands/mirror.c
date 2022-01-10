#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/seat.h"
#include "sway/mirror.h"
#include "sway/output.h"
#include "sway/server.h"
#include "log.h"
#include "util.h"

static char params_failure_message[1024];

static bool test_con_id(struct sway_container *container, void *data) {
	size_t *con_id = data;
	return container->node.id == *con_id;
}

bool build_src_params(struct sway_mirror_params *params, char *src_name_or_id) {

	struct sway_output *output_src = all_output_by_name_or_id(src_name_or_id);
	if (!output_src) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"src_name '%s' not found.", src_name_or_id);
		return false;
	}
	if (!output_src->enabled) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"src_name '%s' not enabled.", src_name_or_id);
		return false;
	}

	params->output_src = output_src->wlr_output;

	return true;
}

bool build_dst_params(struct sway_mirror_params *params, char *dst_name_or_id,
		char *scale_str) {

	struct sway_output *output_dst = all_output_by_name_or_id(dst_name_or_id);
	if (!output_dst) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"dst_name '%s' not found.", dst_name_or_id);
		return false;
	}
	if (mirror_output_is_mirror_dst(output_dst)) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"dst_name '%s' already mirroring.", dst_name_or_id);
		return false;
	}
	if (!output_dst->enabled) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"dst_name '%s' not enabled.", dst_name_or_id);
		return false;
	}
	params->wlr_params.output_dst = output_dst->wlr_output;

	if (!scale_str) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"Invalid scale.");
		return false;
	} else if (strcmp(scale_str, "full") == 0) {
		params->wlr_params.scale = WLR_MIRROR_SCALE_FULL;
	} else if (strcmp(scale_str, "aspect") == 0) {
		params->wlr_params.scale = WLR_MIRROR_SCALE_ASPECT;
	} else if (strcmp(scale_str, "center") == 0) {
		params->wlr_params.scale = WLR_MIRROR_SCALE_CENTER;
	} else {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"Invalid scale '%s', expected <full|aspect|center>.", scale_str);
		return false;
	}

	return true;
}

bool build_focussed_params(struct sway_mirror_params *params) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *container = seat_get_focused_container(seat);
	if (!container) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"No focussed container.");
		return false;
	}
	params->con_id = container->node.id;

	return true;
}

bool build_con_id_params(struct sway_mirror_params *params, char *str) {
	char *end;
	size_t con_id = strtol(str, &end, 10);
	if (end[0] != '\0') {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"Invalid con_id '%s'.", str);
		return false;
	}
	struct sway_container *container = root_find_container(test_con_id, &con_id);
	if (!container) {
		snprintf(params_failure_message, sizeof(params_failure_message),
				"con_id '%ld' not found.", con_id);
		return false;
	}

	params->con_id = con_id;
	return true;
}

struct cmd_results *cmd_mirror_start_entire(int argc, char **argv) {
	const char usage[] = "Expected 'mirror start entire "
		"<dst_name> <full|aspect|center> <src_name> [show_cursor]'";

	if (argc < 3 || argc > 4) {
		return cmd_results_new(CMD_INVALID, usage);
	}

	struct sway_mirror_params params = { 0 };
	params.flavour = SWAY_MIRROR_FLAVOUR_ENTIRE;

	if (!build_dst_params(&params, argv[0], argv[1])) {
		return cmd_results_new(CMD_FAILURE, params_failure_message);
	}

	if (!build_src_params(&params, argv[2])) {
		return cmd_results_new(CMD_FAILURE, params_failure_message);
	}

	if (params.output_src == params.wlr_params.output_dst) {
		return cmd_results_new(CMD_FAILURE, "src and dst must be different");
	}

	if (argc == 4) {
		if (strcmp(argv[3], "show_cursor") != 0) {
			return cmd_results_new(CMD_FAILURE, usage);
		}
		params.wlr_params.overlay_cursor = true;
	}

	if (!mirror_create(&params)) {
		return cmd_results_new(CMD_FAILURE, "Mirror failed to start, check logs.");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_mirror_start_box(int argc, char **argv) {
	const char usage[] = "Expected 'mirror start box "
		"<dst_name> <full|aspect|center> "
		"<x> <y> <width> <height> [show_cursor]'";

	if (argc < 6 || argc > 7) {
		return cmd_results_new(CMD_INVALID, usage);
	}

	struct sway_mirror_params params = { 0 };
	params.flavour = SWAY_MIRROR_FLAVOUR_BOX;

	if (!build_dst_params(&params, argv[0], argv[1])) {
		return cmd_results_new(CMD_FAILURE, params_failure_message);
	}

	char *end;
	params.box.x = strtol(argv[2], &end, 10);
	if (end[0] != '\0' || params.box.x < 0) {
		return cmd_results_new(CMD_FAILURE, "Invalid x '%s'.", argv[2]);
	}
	params.box.y = strtol(argv[3], &end, 10);
	if (end[0] != '\0' || params.box.y < 0) {
		return cmd_results_new(CMD_FAILURE, "Invalid y '%s'.", argv[3]);
	}
	params.box.width = strtol(argv[4], &end, 10);
	if (end[0] != '\0' || params.box.width < 1) {
		return cmd_results_new(CMD_FAILURE, "Invalid width '%s'.", argv[4]);
	}
	params.box.height = strtol(argv[5], &end, 10);
	if (end[0] != '\0' || params.box.height < 1) {
		return cmd_results_new(CMD_FAILURE, "Invalid height '%s'.", argv[5]);
	}

	// find the output for the origin of the box
	params.output_src = wlr_output_layout_output_at(root->output_layout,
			params.box.x, params.box.y);
	if (!params.output_src) {
		return cmd_results_new(CMD_FAILURE, "Box not on any output.");
	}

	// box cannot be on the dst
	if (params.output_src == params.wlr_params.output_dst) {
		return cmd_results_new(CMD_FAILURE, "Box on dst.");
	}

	// convert to local output coordinates, ensuring within output_src
	if (!mirror_layout_box_within_output(&params.box, params.output_src)) {
		return cmd_results_new(CMD_FAILURE, "Box covers multiple outputs.");
	}

	if (argc == 7) {
		if (strcmp(argv[6], "show_cursor") != 0) {
			return cmd_results_new(CMD_FAILURE, usage);
		}
		params.wlr_params.overlay_cursor = true;
	}

	if (!mirror_create(&params)) {
		return cmd_results_new(CMD_FAILURE, "Mirror failed to start, check logs.");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_mirror_start_container(int argc, char **argv) {
	const char usage[] = "Expected 'mirror start container "
		"<dst_name> <full|aspect|center> [con_id] [show_cursor]'";

	if (argc < 2 || argc > 4) {
		return cmd_results_new(CMD_INVALID, usage);
	}

	struct sway_mirror_params params = { 0 };
	params.flavour = SWAY_MIRROR_FLAVOUR_CONTAINER;

	if (!build_dst_params(&params, argv[0], argv[1])) {
		return cmd_results_new(CMD_FAILURE, params_failure_message);
	}

	switch(argc) {
		case 2:
			if (!build_focussed_params(&params)) {
				return cmd_results_new(CMD_FAILURE, params_failure_message);
			}
			break;
		case 3:
			if (strcmp(argv[2], "show_cursor") == 0) {
				params.wlr_params.overlay_cursor = true;
				if (!build_focussed_params(&params)) {
					return cmd_results_new(CMD_FAILURE, params_failure_message);
				}
			} else if (!build_con_id_params(&params, argv[2])) {
				return cmd_results_new(CMD_FAILURE, params_failure_message);
			}
			break;
		case 4:
			if (!build_con_id_params(&params, argv[2])) {
				return cmd_results_new(CMD_FAILURE, params_failure_message);
			}
			if (strcmp(argv[3], "show_cursor") != 0) {
				return cmd_results_new(CMD_FAILURE, usage);
			}
			params.wlr_params.overlay_cursor = true;
			break;
		default:
			break;
	}

	if (!mirror_create(&params)) {
		return cmd_results_new(CMD_FAILURE, "Mirror failed to start, check logs.");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_mirror_start(int argc, char **argv) {
	const char usage[] = "Expected 'mirror start <entire|container|box> ...'";

	if (strcasecmp(argv[0], "entire") == 0) {
		return cmd_mirror_start_entire(argc - 1, &argv[1]);
	} else if (strcasecmp(argv[0], "container") == 0) {
		return cmd_mirror_start_container(argc - 1, &argv[1]);
	} else if (strcasecmp(argv[0], "box") == 0) {
		return cmd_mirror_start_box(argc - 1, &argv[1]);
	} else {
		return cmd_results_new(CMD_INVALID, usage);
	}
}

struct cmd_results *cmd_mirror_stop(void) {
	mirror_destroy_all();
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_mirror(int argc, char **argv) {
	const char usage[] = "Expected 'mirror start <entire|container|box> ...' or "
		"'mirror stop'";

	if (argc < 1) {
		return cmd_results_new(CMD_INVALID, usage);
	}
	if (strcasecmp(argv[0], "stop") == 0) {
		return cmd_mirror_stop();
	}
	if (argc < 2 || strcasecmp(argv[0], "start") != 0) {
		return cmd_results_new(CMD_INVALID, usage);
	}

	return cmd_mirror_start(argc - 1, &argv[1]);
}

