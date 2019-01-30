#include <string.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"

enum operation {
	OP_ENABLE,
	OP_DISABLE,
	OP_ESCAPE,
};

// pointer_constraint [enable|disable|escape]
struct cmd_results *cmd_pointer_constraint(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pointer_constraint", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	enum operation op;
	if (strcmp(argv[0], "enable") == 0) {
		op = OP_ENABLE;
	} else if (strcmp(argv[0], "disable") == 0) {
		op = OP_DISABLE;
	} else if (strcmp(argv[0], "escape") == 0) {
		op = OP_ESCAPE;
	} else {
		return cmd_results_new(CMD_FAILURE, "Expected enable|disable|escape");
	}

	if (op == OP_ESCAPE && config->reading) {
		return cmd_results_new(CMD_FAILURE, "Can only escape at runtime.");
	}

	struct sway_cursor *cursor = config->handler_context.seat->cursor;
	struct seat_config *seat_config = seat_get_config(cursor->seat);
	switch (op) {
	case OP_ENABLE:
		seat_config->allow_constrain = true;
		break;
	case OP_DISABLE:
		seat_config->allow_constrain = false;
		/* fallthrough */
	case OP_ESCAPE:
		sway_cursor_constrain(cursor, NULL);
		break;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
