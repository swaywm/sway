#include "sway/commands.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct cmd_results *cmd_floating_default_width(int argc, char **argv) {
    struct cmd_results *error;
    if ((error =
             checkarg(argc, "floating_default_width", EXPECTED_EQUAL_TO, 1))) {
        return error;
    }

    char *err;
    float width = strtof(argv[0], &err);
    if (*err) {
        return cmd_results_new(CMD_INVALID,
                               "Expected 'floating_default_width <width>'");
    }

    config->floating_default_width = width;

    return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_floating_default_height(int argc, char **argv) {
    struct cmd_results *error;
    if ((error =
             checkarg(argc, "floating_default_height", EXPECTED_EQUAL_TO, 1))) {
        return error;
    }

    char *err;
    float height = strtof(argv[0], &err);
    if (*err) {
        return cmd_results_new(CMD_INVALID,
                               "Expected 'floating_default_height <height>'");
    }

    config->floating_default_height = height;

    return cmd_results_new(CMD_SUCCESS, NULL);
}
