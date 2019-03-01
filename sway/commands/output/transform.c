#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "sway/output.h"

static enum wl_output_transform rotation_to_transform(int deg, bool flipped) {
	// Calculate the modulus.
	int mod = (((deg / 90) % 4) + 4) % 4;
	// The last 4 variants of wl_output_transform are FLIPPED.
	return flipped ? mod + 4 : mod;
}

struct cmd_results *output_cmd_transform(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing transform argument.");
	}

	enum wl_output_transform transform;

	// This allows for:
	// <deg>
	// normal
	// normal-<deg>
	// flipped
	// flipped-<deg>

	if (strncmp(*argv, "normal", 6) == 0) {
		int len = strlen(*argv);
		if (len >= 8) {
			char *end = NULL;
			int deg = strtol(&(*argv)[7], &end, 10);
			if (end == *argv) {
				return cmd_results_new(CMD_INVALID, "Invalid output rotation.");
			}
			transform = rotation_to_transform(deg, false);
		} else {
			transform = WL_OUTPUT_TRANSFORM_NORMAL;
		}
	} else if (strncmp(*argv, "flipped", 7) == 0) {
		int len = strlen(*argv);
		if (len >= 9) {
			char *end = NULL;
			int deg = strtol(&(*argv)[8], &end, 10);
			if (end == *argv) {
				return cmd_results_new(CMD_INVALID, "Invalid output rotation.");
			}
			transform = rotation_to_transform(deg, true);
		} else {
			transform = WL_OUTPUT_TRANSFORM_FLIPPED;
		}
	} else {
		char *end = NULL;
		int deg = strtol(*argv, &end, 10);
		if (end == *argv) {
			// A rotation wasn't passed and none of the other transforms
			// matched, so it's an invalid *transform*.
			return cmd_results_new(CMD_INVALID, "Invalid output transform.");
		}
		transform = rotation_to_transform(deg, false);
	}

	struct output_config *output = config->handler_context.output_config;
	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	if (argc > 1 &&
			(strcmp(argv[1], "clockwise") == 0 || strcmp(argv[1], "anticlockwise") == 0)) {
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
			transform = wlr_output_transform_invert(transform);
		}
		struct wlr_output *w_output = s_output->wlr_output;
		transform = wlr_output_transform_compose(w_output->transform, transform);
		config->handler_context.leftovers.argv += 1;
		config->handler_context.leftovers.argc -= 1;
	}
	output->transform = transform;
	return NULL;
}
