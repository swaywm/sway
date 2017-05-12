#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/focus.h"
#include "sway/layout.h"

struct cmd_results *cmd_gaps(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char *expected_syntax =
		"Expected 'gaps edge_gaps <on|off|toggle>' or "
		"'gaps <inner|outer> <current|all|workspace> <set|plus|minus n>'";
	const char *amount_str = argv[0];
	// gaps amount
	if (argc >= 1 && isdigit(*amount_str)) {
		int amount = (int)strtol(amount_str, NULL, 10);
		if (errno == ERANGE) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "gaps", "Number is out out of range.");
		}
		config->gaps_inner = config->gaps_outer = amount;
		arrange_windows(&root_container, -1, -1);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	// gaps inner|outer n
	else if (argc >= 2 && isdigit((amount_str = argv[1])[0])) {
		int amount = (int)strtol(amount_str, NULL, 10);
		if (errno == ERANGE) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "gaps", "Number is out out of range.");
		}
		const char *target_str = argv[0];
		if (strcasecmp(target_str, "inner") == 0) {
			config->gaps_inner = amount;
		} else if (strcasecmp(target_str, "outer") == 0) {
			config->gaps_outer = amount;
		}
		arrange_windows(&root_container, -1, -1);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (argc == 2 && strcasecmp(argv[0], "edge_gaps") == 0) {
		// gaps edge_gaps <on|off|toggle>
		if (strcasecmp(argv[1], "toggle") == 0) {
			if (config->reading) {
				return cmd_results_new(CMD_FAILURE, "gaps edge_gaps toggle",
					"Can't be used in config file.");
			}
			config->edge_gaps = !config->edge_gaps;
		} else {
			config->edge_gaps =
				(strcasecmp(argv[1], "yes") == 0 || strcasecmp(argv[1], "on") == 0);
		}
		arrange_windows(&root_container, -1, -1);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	// gaps inner|outer current|all set|plus|minus n
	if (argc < 4 || config->reading) {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}
	// gaps inner|outer ...
	const char *inout_str = argv[0];
	enum {INNER, OUTER} inout;
	if (strcasecmp(inout_str, "inner") == 0) {
		inout = INNER;
	} else if (strcasecmp(inout_str, "outer") == 0) {
		inout = OUTER;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}

	// gaps ... current|all ...
	const char *target_str = argv[1];
	enum {CURRENT, WORKSPACE, ALL} target;
	if (strcasecmp(target_str, "current") == 0) {
		target = CURRENT;
	} else if (strcasecmp(target_str, "all") == 0) {
		target = ALL;
	} else if (strcasecmp(target_str, "workspace") == 0) {
		if (inout == OUTER) {
			target = CURRENT;
		} else {
			// Set gap for views in workspace
			target = WORKSPACE;
		}
	} else {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}

	// gaps ... n
	amount_str = argv[3];
	int amount = (int)strtol(amount_str, NULL, 10);
	if (errno == ERANGE) {
		errno = 0;
		return cmd_results_new(CMD_INVALID, "gaps", "Number is out out of range.");
	}

	// gaps ... set|plus|minus ...
	const char *method_str = argv[2];
	enum {SET, ADD} method;
	if (strcasecmp(method_str, "set") == 0) {
		method = SET;
	} else if (strcasecmp(method_str, "plus") == 0) {
		method = ADD;
	} else if (strcasecmp(method_str, "minus") == 0) {
		method = ADD;
		amount *= -1;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}

	if (target == CURRENT) {
		swayc_t *cont;
		if (inout == OUTER) {
			if ((cont = swayc_active_workspace()) == NULL) {
				return cmd_results_new(CMD_FAILURE, "gaps", "There's no active workspace.");
			}
		} else {
			if ((cont = get_focused_view(&root_container))->type != C_VIEW) {
				return cmd_results_new(CMD_FAILURE, "gaps", "Currently focused item is not a view.");
			}
		}
		cont->gaps = swayc_gap(cont);
		if (method == SET) {
			cont->gaps = amount;
		} else if ((cont->gaps += amount) < 0) {
			cont->gaps = 0;
		}
		arrange_windows(cont->parent, -1, -1);
	} else if (inout == OUTER) {
		//resize all workspace.
		for (size_t i = 0; i < root_container.children->length; ++i) {
			swayc_t *op = *(swayc_t **)list_get(root_container.children, i);
			for (size_t j = 0; j < op->children->length; ++j) {
				swayc_t *ws = *(swayc_t **)list_get(op->children, j);
				if (method == SET) {
					ws->gaps = amount;
				} else if ((ws->gaps += amount) < 0) {
					ws->gaps = 0;
				}
			}
		}
		arrange_windows(&root_container, -1, -1);
	} else {
		// Resize gaps for all views in workspace
		swayc_t *top;
		if (target == WORKSPACE) {
			if ((top = swayc_active_workspace()) == NULL) {
				return cmd_results_new(CMD_FAILURE, "gaps", "There's currently no active workspace.");
			}
		} else {
			top = &root_container;
		}
		int top_gap = top->gaps;
		container_map(top, method == SET ? set_gaps : add_gaps, &amount);
		top->gaps = top_gap;
		arrange_windows(top, -1, -1);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
