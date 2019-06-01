#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *seat_cmd_xcursor_theme(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xcursor_theme", EXPECTED_AT_LEAST, 1)) ||
		(error = checkarg(argc, "xcursor_theme", EXPECTED_AT_MOST, 2))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	const char *theme_name = argv[0];
	unsigned size = 24;

	if (argc == 2) {
		char *end;
		size = strtoul(argv[1], &end, 10);
		if (*end) {
			return cmd_results_new(
				CMD_INVALID, "Expected a positive integer size");
		}
	}

	free(config->handler_context.seat_config->xcursor_theme.name);
	config->handler_context.seat_config->xcursor_theme.name = strdup(theme_name);
	config->handler_context.seat_config->xcursor_theme.size = size;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
