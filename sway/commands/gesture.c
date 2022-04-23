#define _POSIX_C_SOURCE 200809L
#include "sway/config.h"

#include "gesture.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"

void free_gesture_binding(struct sway_gesture_binding *binding) {
	if (!binding) {
		return;
	}
	free(binding->input);
	free(binding->command);
	free(binding);
}

/**
 * Returns true if the bindings have the same gesture type, direction, etc
 */
static bool binding_gesture_equal(struct sway_gesture_binding *binding_a,
								  struct sway_gesture_binding *binding_b) {
	if (strcmp(binding_a->input, binding_b->input) != 0) {
		return false;
	}

	if (!gesture_equal(&binding_a->gesture, &binding_b->gesture)) {
		return false;
	}

	if ((binding_a->flags & BINDING_EXACT) !=
		(binding_b->flags & BINDING_EXACT)) {
		return false;
	}
	return true;
}

/**
 * Add gesture binding to config
 */
static struct cmd_results *gesture_binding_add(
		struct sway_gesture_binding *binding,
		const char *gesturecombo, bool warn) {
	list_t *mode_bindings = config->current_mode->gesture_bindings;
	// overwrite the binding if it already exists
	bool overwritten = false;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_gesture_binding *config_binding = mode_bindings->items[i];
		if (binding_gesture_equal(binding, config_binding)) {
			sway_log(SWAY_INFO, "Overwriting binding '%s' to `%s` from `%s`",
					gesturecombo, binding->command, config_binding->command);
			if (warn) {
				config_add_swaynag_warning("Overwriting binding"
						"'%s' to `%s` from `%s`",
						gesturecombo, binding->command,
						config_binding->command);
			}
			free_gesture_binding(config_binding);
			mode_bindings->items[i] = binding;
			overwritten = true;
		}
	}

	if (!overwritten) {
		list_add(mode_bindings, binding);
		sway_log(SWAY_DEBUG, "bindgesture - Bound %s to command `%s`",
				gesturecombo, binding->command);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Remove gesture binding from config
 */
static struct cmd_results *gesture_binding_remove(
		struct sway_gesture_binding *binding, const char *gesturecombo) {
	list_t *mode_bindings = config->current_mode->gesture_bindings;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_gesture_binding *config_binding = mode_bindings->items[i];
		if (binding_gesture_equal(binding, config_binding)) {
			free_gesture_binding(config_binding);
			free_gesture_binding(binding);
			list_del(mode_bindings, i);
			sway_log(SWAY_DEBUG, "unbindgesture - Unbound %s gesture",
					gesturecombo);
			return cmd_results_new(CMD_SUCCESS, NULL);
		}
	}

	free_gesture_binding(binding);
	return cmd_results_new(CMD_FAILURE, "Could not find gesture binding `%s`",
			gesturecombo);
}

/**
 * Parse and execute bindgesture or unbindgesture command.
 */
static struct cmd_results *cmd_bind_or_unbind_gesture(int argc, char **argv, bool unbind) {
	int minargs = 2;
	char *bindtype = "bindgesture";
	if (unbind) {
		minargs--;
		bindtype = "unbindgesture";
	}

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, bindtype, EXPECTED_AT_LEAST, minargs))) {
		return error;
	}
	struct sway_gesture_binding *binding = calloc(1, sizeof(struct sway_gesture_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate binding");
	}
	binding->input = strdup("*");

	bool warn = true;

	// Handle flags
	while (argc > 0) {
		if (strcmp("--exact", argv[0]) == 0) {
			binding->flags |= BINDING_EXACT;
		} else if (strcmp("--no-warn", argv[0]) == 0) {
			warn = false;
		} else if (strncmp("--input-device=", argv[0],
					strlen("--input-device=")) == 0) {
			free(binding->input);
			binding->input = strdup(argv[0] + strlen("--input-device="));
		} else {
			break;
		}
		argv++;
		argc--;
	}

	if (argc < minargs) {
		free(binding);
		return cmd_results_new(CMD_FAILURE,
				"Invalid %s command (expected at least %d "
				"non-option arguments, got %d)", bindtype, minargs, argc);
	}

	char* errmsg = NULL;
	if ((errmsg = gesture_parse(argv[0], &binding->gesture))) {
		free(binding);
		struct cmd_results *final = cmd_results_new(CMD_FAILURE,
				"Invalid %s command (%s)",
				bindtype, errmsg);
		free(errmsg);
		return final;
	}

	if (unbind) {
		return gesture_binding_remove(binding, argv[0]);
	}
	binding->command = join_args(argv + 1, argc - 1);
	return gesture_binding_add(binding, argv[0], warn);
}

struct cmd_results *cmd_bindgesture(int argc, char **argv) {
	return cmd_bind_or_unbind_gesture(argc, argv, false);
}

struct cmd_results *cmd_unbindgesture(int argc, char **argv) {
	return cmd_bind_or_unbind_gesture(argc, argv, true);
}
