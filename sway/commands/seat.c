#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"
#include "stringop.h"

// must be in order for the bsearch
static struct cmd_handler seat_handlers[] = {
	{ "attach", seat_cmd_attach },
	{ "cursor", seat_cmd_cursor },
	{ "fallback", seat_cmd_fallback },
};

struct cmd_results *cmd_seat(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "seat", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	config->handler_context.seat_config = new_seat_config(argv[0]);
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, NULL,
				"Couldn't allocate config");
	}

	struct cmd_results *res = subcommand(argv + 1, argc - 1, seat_handlers,
			sizeof(seat_handlers));

	free_seat_config(config->handler_context.seat_config);
	config->handler_context.seat_config = NULL;

	return res;
}
