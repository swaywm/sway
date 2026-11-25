#include "sway/config.h"
#include "sway/criteria.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"

void free_bell_binding(struct sway_bell_binding *binding) {
	if (!binding) {
		return;
	}
	if (binding->criteria) {
		criteria_destroy(binding->criteria);
	}
	free(binding->command);
	free(binding);
}

static bool binding_bell_equal(struct sway_bell_binding *binding_a,
		struct sway_bell_binding *binding_b) {
	if (strcmp(binding_a->command, binding_b->command) != 0) {
		return false;
	}
	if ((binding_a->criteria == NULL) != (binding_b->criteria == NULL)) {
		return false;
	}
	if (binding_a->criteria && strcmp(binding_a->criteria->raw,
				binding_b->criteria->raw) != 0) {
		return false;
	}
	return true;
}

static struct cmd_results *bell_binding_add(
		struct sway_bell_binding *binding, bool warn) {
	list_t *mode_bindings = config->current_mode->bell_bindings;
	bool overwritten = false;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_bell_binding *config_binding = mode_bindings->items[i];
		if (binding_bell_equal(binding, config_binding)) {
			sway_log(SWAY_INFO, "Overwriting bell binding to `%s` from `%s`",
					binding->command, config_binding->command);
			if (warn) {
				config_add_swaynag_warning("Overwriting bell binding"
										   " to `%s` from `%s`",
						binding->command, config_binding->command);
			}
			free_bell_binding(config_binding);
			mode_bindings->items[i] = binding;
			overwritten = true;
		}
	}

	if (!overwritten) {
		list_add(mode_bindings, binding);
		sway_log(SWAY_DEBUG, "bindbell - Bound to command `%s`",
				binding->command);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *bell_binding_remove(
		struct sway_bell_binding *binding) {
	list_t *mode_bindings = config->current_mode->bell_bindings;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_bell_binding *config_binding = mode_bindings->items[i];
		if (binding_bell_equal(binding, config_binding)) {
			free_bell_binding(config_binding);
			free_bell_binding(binding);
			list_del(mode_bindings, i);
			sway_log(SWAY_DEBUG, "unbindbell - Unbound bell binding");
			return cmd_results_new(CMD_SUCCESS, NULL);
		}
	}

	free_bell_binding(binding);
	return cmd_results_new(CMD_FAILURE, "Could not find bell binding");
}

static struct cmd_results *cmd_bind_or_unbind_bell(int argc, char **argv, bool unbind) {
	char *bindtype = unbind ? "unbindbell" : "bindbell";

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, bindtype, EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	bool warn = true;
	struct criteria *criteria = NULL;
	int i = 0;
	while (i < argc) {
		if (!unbind && strcmp("--no-warn", argv[i]) == 0) {
			warn = false;
			i++;
		} else if (argv[i][0] == '[' && !criteria) {
			char *err_str = NULL;
			criteria = criteria_parse(argv[i], &err_str);
			if (!criteria) {
				error = cmd_results_new(CMD_INVALID, "%s", err_str);
				free(err_str);
				return error;
			}
			i++;
		} else {
			break;
		}
	}

	if (i >= argc) {
		if (criteria) {
			criteria_destroy(criteria);
		}
		return cmd_results_new(CMD_FAILURE,
				"Invalid %s command (expected a command)", bindtype);
	}

	struct sway_bell_binding *binding = calloc(1, sizeof(struct sway_bell_binding));
	if (!binding) {
		if (criteria) {
			criteria_destroy(criteria);
		}
		return cmd_results_new(CMD_FAILURE, "Unable to allocate binding");
	}
	binding->command = join_args(argv + i, argc - i);
	binding->criteria = criteria;

	if (unbind) {
		return bell_binding_remove(binding);
	}
	return bell_binding_add(binding, warn);
}

struct cmd_results *cmd_bindbell(int argc, char **argv) {
	return cmd_bind_or_unbind_bell(argc, argv, false);
}

struct cmd_results *cmd_unbindbell(int argc, char **argv) {
	return cmd_bind_or_unbind_bell(argc, argv, true);
}
