#include <stdlib.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/server.h"

struct cmd_results *seat_cmd_edge_resistance(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "edge_resistance", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct seat_config *sc = config->handler_context.seat_config;
	if (!sc) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	char *end;
	int val = strtol(argv[0], &end, 10);
	if (*end || val < 0) {
		return cmd_results_new(CMD_INVALID,
			"edge_resistance: expected a non-negative integer");
	}
	sc->edge_resistance = val;

	// Invalidate cursor cache for all seats
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat->cursor->cached_edge_resistance = -1;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
