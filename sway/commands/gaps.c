#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "log.h"
#include "stringop.h"
#include <math.h>

enum gaps_op {
	GAPS_OP_SET,
	GAPS_OP_ADD,
	GAPS_OP_SUBTRACT
};

enum gaps_scope {
	GAPS_SCOPE_ALL,
	GAPS_SCOPE_WORKSPACE,
	GAPS_SCOPE_CURRENT
};

struct cmd_results *cmd_gaps(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1);
	if (error) {
		return error;
	}

	if (strcmp(argv[0], "edge_gaps") == 0) {
		if ((error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 2))) {
			return error;
		}

		if (strcmp(argv[1], "on") == 0) {
			config->edge_gaps = true;
		} else if (strcmp(argv[1], "off") == 0) {
			config->edge_gaps = false;
		} else if (strcmp(argv[1], "toggle") == 0) {
			if (!config->active) {
				return cmd_results_new(CMD_INVALID, "gaps",
					"Cannot toggle gaps while not running.");
			}
			config->edge_gaps = !config->edge_gaps;
		} else {
			return cmd_results_new(CMD_INVALID, "gaps",
				"gaps edge_gaps on|off|toggle");
		}
		arrange_and_commit(&root_container);
	} else {
		int amount_idx = 0; // the current index in argv
		enum gaps_op op = GAPS_OP_SET;
		enum gaps_scope scope = GAPS_SCOPE_ALL;
		bool inner = true;

		if (strcmp(argv[0], "inner") == 0) {
			amount_idx++;
			inner = true;
		} else if (strcmp(argv[0], "outer") == 0) {
			amount_idx++;
			inner = false;
		}

		// If one of the long variants of the gaps command is used
		// (which starts with inner|outer) check the number of args
		if (amount_idx > 0) { // if we've seen inner|outer
			if (argc > 2) { // check the longest variant
				error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 4);
				if (error) {
					return error;
				}
			} else { // check the next longest format
				error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 2);
				if (error) {
					return error;
				}
			}
		} else {
			error = checkarg(argc, "gaps", EXPECTED_EQUAL_TO, 1);
			if (error) {
				return error;
			}
		}

		if (argc == 4) {
			// Long format: all|workspace|current.
			if (strcmp(argv[amount_idx], "all") == 0) {
				amount_idx++;
				scope = GAPS_SCOPE_ALL;
			} else if (strcmp(argv[amount_idx], "workspace") == 0) {
				amount_idx++;
				scope = GAPS_SCOPE_WORKSPACE;
			} else if (strcmp(argv[amount_idx], "current") == 0) {
				amount_idx++;
				scope = GAPS_SCOPE_CURRENT;
			}

			// Long format: set|plus|minus
			if (strcmp(argv[amount_idx], "set") == 0) {
				amount_idx++;
				op = GAPS_OP_SET;
			} else if (strcmp(argv[amount_idx], "plus") == 0) {
				amount_idx++;
				op = GAPS_OP_ADD;
			} else if (strcmp(argv[amount_idx], "minus") == 0) {
				amount_idx++;
				op = GAPS_OP_SUBTRACT;
			}
		}

		char *end;
		double val = strtod(argv[amount_idx], &end);

		if (strlen(end) && val == 0.0) { // invalid <amount>
			// guess which variant of the command was attempted
			if (argc == 1) {
				return cmd_results_new(CMD_INVALID, "gaps", "gaps <amount>");
			}
			if (argc == 2) {
				return cmd_results_new(CMD_INVALID, "gaps",
					"gaps inner|outer <amount>");
			}
			return cmd_results_new(CMD_INVALID, "gaps",
				"gaps inner|outer all|workspace|current set|plus|minus <amount>");
		}

		if (amount_idx == 0) { // gaps <amount>
			config->gaps_inner = val;
			config->gaps_outer = val;
			arrange_and_commit(&root_container);
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}
		// Other variants. The middle-length variant (gaps inner|outer <amount>)
		// just defaults the scope to "all" and defaults the op to "set".

		double total;
		switch (op) {
			case GAPS_OP_SUBTRACT: {
				total = (inner ? config->gaps_inner : config->gaps_outer) - val;
				if (total < 0) {
					total = 0;
				}
				break;
			}
			case GAPS_OP_ADD: {
				total = (inner ? config->gaps_inner : config->gaps_outer) + val;
				break;
			}
			case GAPS_OP_SET: {
				total = val;
				break;
			}
		}

		if (scope == GAPS_SCOPE_ALL) {
			if (inner) {
				config->gaps_inner = total;
			} else {
				config->gaps_outer = total;
			}
			arrange_and_commit(&root_container);
		} else {
			struct sway_container *c =
				config->handler_context.current_container;
			if (scope == GAPS_SCOPE_WORKSPACE && c->type != C_WORKSPACE) {
				c = container_parent(c, C_WORKSPACE);
			}
			c->has_gaps = true;
			if (inner) {
				c->gaps_inner = total;
			} else {
				c->gaps_outer = total;
			}

			arrange_and_commit(c->parent ? c->parent : &root_container);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
