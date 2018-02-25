#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *cmd_seat(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "seat", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[1]) == 0) {
		free_seat_config(config->handler_context.seat_config);
		config->handler_context.seat_config = new_seat_config(argv[0]);
		if (!config->handler_context.seat_config) {
			return cmd_results_new(CMD_FAILURE, NULL, "Couldn't allocate config");
		}
		wlr_log(L_DEBUG, "entering seat block: %s", argv[0]);
		return cmd_results_new(CMD_BLOCK_SEAT, NULL, NULL);
	}

	if ((error = checkarg(argc, "seat", EXPECTED_AT_LEAST, 3))) {
		return error;
	}

	bool has_context = (config->handler_context.seat_config != NULL);
	if (!has_context) {
		config->handler_context.seat_config = new_seat_config(argv[0]);
		if (!config->handler_context.seat_config) {
			return cmd_results_new(CMD_FAILURE, NULL, "Couldn't allocate config");
		}
	}

	int argc_new = argc-2;
	char **argv_new = argv+2;

	struct cmd_results *res;
	if (strcasecmp("attach", argv[1]) == 0) {
		res = seat_cmd_attach(argc_new, argv_new);
	} else if (strcasecmp("fallback", argv[1]) == 0) {
		res = seat_cmd_fallback(argc_new, argv_new);
	} else {
		res = cmd_results_new(CMD_INVALID, "seat <name>", "Unknown command %s", argv[1]);
	}

	if (!has_context) {
		// clean up the context we created earlier
		free_seat_config(config->handler_context.seat_config);
		config->handler_context.seat_config = NULL;
	}

	return res;
}
