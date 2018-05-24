#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

// sort in order of longest->shortest
static int compare_set_qsort(const void *_l, const void *_r) {
	struct sway_variable const *l = *(void **)_l;
	struct sway_variable const *r = *(void **)_r;
	return strlen(r->name) - strlen(l->name);
}

void free_sway_variable(struct sway_variable *var) {
	if (!var) {
		return;
	}
	free(var->name);
	free(var->value);
	free(var);
}

struct cmd_results *cmd_set(int argc, char **argv) {
	char *tmp;
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "set", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (argv[0][0] != '$') {
		sway_log(L_INFO, "Warning: variable '%s' doesn't start with $", argv[0]);

		size_t size = snprintf(NULL, 0, "$%s", argv[0]);
		tmp = malloc(size + 1);
		if (!tmp) {
			return cmd_results_new(CMD_FAILURE, "set", "Not possible to create variable $'%s'", argv[0]);
		}
		snprintf(tmp, size+1, "$%s", argv[0]);

		argv[0] = tmp;
	}

	struct sway_variable *var = NULL;
	// Find old variable if it exists
	int i;
	for (i = 0; i < config->symbols->length; ++i) {
		var = config->symbols->items[i];
		if (strcmp(var->name, argv[0]) == 0) {
			break;
		}
		var = NULL;
	}
	if (var) {
		free(var->value);
	} else {
		var = malloc(sizeof(struct sway_variable));
		if (!var) {
			return cmd_results_new(CMD_FAILURE, "set", "Unable to allocate variable");
		}
		var->name = strdup(argv[0]);
		list_add(config->symbols, var);
		list_qsort(config->symbols, compare_set_qsort);
	}
	var->value = join_args(argv + 1, argc - 1);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
