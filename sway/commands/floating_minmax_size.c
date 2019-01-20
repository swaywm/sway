#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

static const char min_usage[] =
	"Expected 'floating_minimum_size <width> x <height>'";

static const char max_usage[] =
	"Expected 'floating_maximum_size <width> x <height>'";

static struct cmd_results *handle_command(int argc, char **argv, char *cmd_name,
		const char *usage, int *config_width, int *config_height) {
	struct cmd_results *error;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	char *err;
	int width = (int)strtol(argv[0], &err, 10);
	if (*err) {
		return cmd_results_new(CMD_INVALID, cmd_name, usage);
	}

	if (strcmp(argv[1], "x") != 0) {
		return cmd_results_new(CMD_INVALID, cmd_name, usage);
	}

	int height = (int)strtol(argv[2], &err, 10);
	if (*err) {
		return cmd_results_new(CMD_INVALID, cmd_name, usage);
	}

	*config_width = width;
	*config_height = height;

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_floating_minimum_size(int argc, char **argv) {
	return handle_command(argc, argv, "floating_minimum_size", min_usage,
			&config->floating_minimum_width, &config->floating_minimum_height);
}

struct cmd_results *cmd_floating_maximum_size(int argc, char **argv) {
	return handle_command(argc, argv, "floating_maximum_size", max_usage,
			&config->floating_maximum_width, &config->floating_maximum_height);
}
