#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 1
#define __BSD_VISIBLE 1
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

// export TERM xterm-256color
struct cmd_results *cmd_export(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "export", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	char *search_var;
	if (asprintf(&search_var, "$ENV:%s", argv[0]) < 0) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate search variable");
	}

	struct sway_variable *var = NULL;
	// Find old variable if it exists
	int i;
	for (i = 0; i < config->symbols->length; ++i) {
		var = config->symbols->items[i];
		if (strcmp(var->name, search_var) == 0) {
			break;
		}
		var = NULL;
	}
	if (var) {
		free(var->value);
	} else {
		var = malloc(sizeof(struct sway_variable));
		if (!var) {
			return cmd_results_new(CMD_FAILURE, "Unable to allocate variable");
		}
		var->name = strdup(search_var);
		list_add(config->symbols, var);
		list_qsort(config->symbols, compare_set_qsort);
	}
	var->value = join_args(argv + 1, argc - 1);
	// NOTE(jbenden): Should an empty string mean to unsetenv?
	if (setenv(argv[0], var->value, true) != 0)
		return cmd_results_new(CMD_FAILURE, "Unable to set environment variable");
	return cmd_results_new(CMD_SUCCESS, NULL);
}
