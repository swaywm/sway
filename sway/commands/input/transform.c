#include "log.h"
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"
#include <strings.h>

static enum sway_input_transform invert_rotation_direction(
		enum sway_input_transform t) {
	switch (t) {
	case INPUT_TRANSFORM_90:
		return INPUT_TRANSFORM_270;
	case INPUT_TRANSFORM_270:
		return INPUT_TRANSFORM_90;
	case INPUT_TRANSFORM_FLIPPED_90:
		return INPUT_TRANSFORM_FLIPPED_270;
	case INPUT_TRANSFORM_FLIPPED_270:
		return INPUT_TRANSFORM_FLIPPED_90;
	default:
		return t;
	}
}

struct cmd_results *input_cmd_transform(int argc, char**argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "transform", EXPECTED_AT_LEAST, 1))) {
        return error;
    }
    
    enum sway_input_transform transform;
    if (strcmp(*argv, "normal") == 0 ||
			strcmp(*argv, "0") == 0) {
		transform = INPUT_TRANSFORM_NORMAL;
	} else if (strcmp(*argv, "90") == 0) {
		transform = INPUT_TRANSFORM_90;
	} else if (strcmp(*argv, "180") == 0) {
		transform = INPUT_TRANSFORM_180;
	} else if (strcmp(*argv, "270") == 0) {
		transform = INPUT_TRANSFORM_270;
	} else if (strcmp(*argv, "flipped") == 0) {
		transform = INPUT_TRANSFORM_FLIPPED;
	} else if (strcmp(*argv, "flipped-90") == 0) {
		transform = INPUT_TRANSFORM_FLIPPED_90;
	} else if (strcmp(*argv, "flipped-180") == 0) {
		transform = INPUT_TRANSFORM_FLIPPED_180;
	} else if (strcmp(*argv, "flipped-270") == 0) {
		transform = INPUT_TRANSFORM_FLIPPED_270;
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid output transform.");
	}

    struct input_config *ic = config->handler_context.input_config;
    if (!ic) {
        return cmd_results_new(CMD_FAILURE, "No input device defined.");
    }

    if (argc > 1) {
        if (strcmp(argv[1], "clockwise") && strcmp(argv[1], "anticlockwise")) {
            return cmd_results_new(CMD_INVALID, "Invalid transform direction");
        }
        if (config->reloading) {
            return cmd_results_new(CMD_INVALID,
                "Relative transforms cannot be used in the configuration file");
        }
        if (strcmp(ic->identifier, "*") == 0) {
            return cmd_results_new(CMD_INVALID, 
                "Cannot apply relative transforms to all inputs");
        }
        if (strcmp(argv[1], "anticlockwise") == 0) {
            transform = invert_rotation_direction(transform);
        }
    }

    ic->transform = transform;

    return cmd_results_new(CMD_SUCCESS, NULL);
}