#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/seat.h"

static const struct {
	const char *name;
	uint32_t value;
} idle_source_strings[] = {
	{ "keyboard", IDLE_SOURCE_KEYBOARD },
	{ "pointer", IDLE_SOURCE_POINTER },
	{ "touch", IDLE_SOURCE_TOUCH },
	{ "tablet_pad", IDLE_SOURCE_TABLET_PAD },
	{ "tablet_tool", IDLE_SOURCE_TABLET_TOOL },
	{ "switch", IDLE_SOURCE_SWITCH },
};

static uint32_t parse_sources(int argc, char **argv) {
	uint32_t sources = 0;
	for (int i = 0; i < argc; ++i) {
		uint32_t value = 0;
		for (size_t j = 0; j < sizeof(idle_source_strings)
				/ sizeof(idle_source_strings[0]); ++j) {
			if (strcasecmp(idle_source_strings[j].name, argv[i]) == 0) {
				value = idle_source_strings[j].value;
				break;
			}
		}
		if (value == 0) {
			return UINT32_MAX;
		}
		sources |= value;
	}
	return sources;
}

struct cmd_results *seat_cmd_idle_inhibit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "idle_inhibit", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	uint32_t sources = parse_sources(argc, argv);
	if (sources == UINT32_MAX) {
		return cmd_results_new(CMD_FAILURE, "Invalid idle source");
	}
	config->handler_context.seat_config->idle_inhibit_sources = sources;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *seat_cmd_idle_wake(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "idle_wake", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	uint32_t sources = parse_sources(argc, argv);
	if (sources == UINT32_MAX) {
		return cmd_results_new(CMD_FAILURE, "Invalid idle source");
	}
	config->handler_context.seat_config->idle_wake_sources = sources;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
