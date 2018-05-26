#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "log.h"
#include "stringop.h"

int read_double(char *str, double *out) {
  char *end;
  double v = strtod(str, &end);
  if (v == -HUGE_VAL || v == HUGE_VAL || strlen(end) > 0) {
    *out = 0.0;
    return 1;
  }
  *out = v;
  return 0;
}

struct cmd_results *cmd_gaps(int argc, char **argv) {
  struct cmd_results *error = NULL;
  if ((error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1))) {
    return error;
  }

  if (strcmp(argv[0], "edge_gaps") == 0) {
    if (strcmp(argv[1], "on") == 0) {
      config->edge_gaps = true;
      arrange_root();
    } else if (strcmp(argv[1], "off") == 0) {
      config->edge_gaps = false;
      arrange_root();
    } else if (strcmp(argv[1], "toggle") == 0) {
      if (config->active) {
	config->edge_gaps = !(config->edge_gaps);
	arrange_root();
      } else {
	return cmd_results_new(CMD_INVALID, "gaps", "Cannot toggle gaps while not running.");
      }
    } else {
      return cmd_results_new(CMD_INVALID, "gaps", "");
    }
  } else {
    int amount_idx = 0;
    char op = '=';
    char scope = 'a';
    bool inner = true;

    if (strcmp(argv[amount_idx], "inner") == 0) {
      amount_idx++;
      inner = true;
    } else if (strcmp(argv[amount_idx], "outer") == 0) {
      amount_idx++;
      inner = false;
    }

    if (amount_idx > 0) {
	    if (strcmp(argv[0], "all") == 0) {
	      amount_idx++;
	      scope = 'a';
	    } else if (strcmp(argv[0], "workspace") == 0) {
	      amount_idx++;
	      scope = 'w';
	    } else if (strcmp(argv[0], "current") == 0) {
	      amount_idx++;
	      scope = 'c';
	    }

	    if (strcmp(argv[amount_idx], "set") == 0) {
	      amount_idx++;
	      op = '=';
	    } else if (strcmp(argv[amount_idx], "plus") == 0) {
	      amount_idx++;
	      op = '+';
	    } else if (strcmp(argv[amount_idx], "minus") == 0) {
	      amount_idx++;
	      op = '-';
	    }
    }

    double val;
    if (read_double(argv[amount_idx], &val) == 0) {
	if (amount_idx == 0) {
	  config->gaps_inner = val;
	  config->gaps_outer = val;
	  arrange_root();
	} else {
	  double total = val;
	  if (op == '-') {
	    total = (inner ? config->gaps_inner : config->gaps_outer) - val;
	    if (total < 0) {
	      total = 0;
	    }
	  } else if (op == '+') {
	    total = (inner ? config->gaps_inner : config->gaps_inner) + val;
	  }

	  if (scope == 'a') {
	    if (inner) {
	      config->gaps_inner = total;
	    } else {
	      config->gaps_outer = total;
	    }
	    arrange_root();
	  } else if (scope == 'w') {
	    struct sway_container *workspace =
	      config->handler_context.current_container;
	    if (workspace->type != C_WORKSPACE) {
	      workspace = container_parent(workspace, C_WORKSPACE);
	    }
	    workspace->has_gaps = true;
	    if (inner) {
	      workspace->gaps_inner = total;
	    } else {
	      workspace->gaps_outer = total;
	    }
	    arrange_workspace(workspace);
	  } else if (scope == 'c') {
	    struct sway_container *container =
	      config->handler_context.current_container;
	    container->has_gaps = true;
	    if (inner) {
	      container->gaps_inner = total;
	    } else {
	      container->gaps_outer = total;
	    }
	    arrange_workspace(container_parent(container, C_WORKSPACE));
	  }
	}
    }
  }

  return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
