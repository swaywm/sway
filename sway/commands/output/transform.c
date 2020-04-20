#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "sway/output.h"

static enum wl_output_transform invert_rotation_direction(
		enum wl_output_transform t) {
	switch (t) {
	case WL_OUTPUT_TRANSFORM_90:
		return WL_OUTPUT_TRANSFORM_270;
	case WL_OUTPUT_TRANSFORM_270:
		return WL_OUTPUT_TRANSFORM_90;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		return WL_OUTPUT_TRANSFORM_FLIPPED_270;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		return WL_OUTPUT_TRANSFORM_FLIPPED_90;
	default:
		return t;
	}
}

struct cmd_results *output_cmd_transform(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing transform argument.");
	}

	enum wl_output_transform transform;
	if (strcmp(*argv, "normal") == 0 ||
			strcmp(*argv, "0") == 0) {
		transform = WL_OUTPUT_TRANSFORM_NORMAL;
	} else if (strcmp(*argv, "90") == 0) {
		transform = WL_OUTPUT_TRANSFORM_90;
	} else if (strcmp(*argv, "180") == 0) {
		transform = WL_OUTPUT_TRANSFORM_180;
	} else if (strcmp(*argv, "270") == 0) {
		transform = WL_OUTPUT_TRANSFORM_270;
	} else if (strcmp(*argv, "flipped") == 0) {
		transform = WL_OUTPUT_TRANSFORM_FLIPPED;
	} else if (strcmp(*argv, "flipped-90") == 0) {
		transform = WL_OUTPUT_TRANSFORM_FLIPPED_90;
	} else if (strcmp(*argv, "flipped-180") == 0) {
		transform = WL_OUTPUT_TRANSFORM_FLIPPED_180;
	} else if (strcmp(*argv, "flipped-270") == 0) {
		transform = WL_OUTPUT_TRANSFORM_FLIPPED_270;
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid output transform.");
	}

	// Sway uses clockwise transforms, while WL_OUTPUT_TRANSFORM_* describe
	// anti-clockwise transforms
	transform = invert_rotation_direction(transform);

	struct output_config *output = config->handler_context.output_config;
	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	if (argc > 1 &&
			(strcmp(argv[1], "clockwise") == 0 || strcmp(argv[1], "anticlockwise") == 0)) {
		if (config->reloading) {
			return cmd_results_new(CMD_INVALID,
				"Relative transforms cannot be used in the configuration file");
		}
		if (!sway_assert(output->name != NULL, "Output config name not set")) {
			return NULL;
		}
		if (strcmp(output->name, "*") == 0) {
			return cmd_results_new(CMD_INVALID,
				"Cannot apply relative transform to all outputs.");
		}
		struct sway_output *s_output = output_by_name_or_id(output->name);
		if (s_output == NULL) {
			return cmd_results_new(CMD_INVALID,
				"Cannot apply relative transform to unknown output %s", output->name);
		}
		if (strcmp(argv[1], "anticlockwise") == 0) {
			transform = invert_rotation_direction(transform);
		}
		struct wlr_output *w_output = s_output->wlr_output;
		transform = wlr_output_transform_compose(w_output->transform, transform);
		config->handler_context.leftovers.argv += 1;
		config->handler_context.leftovers.argc -= 1;
	}

	output->transform = transform;
	return NULL;
}
