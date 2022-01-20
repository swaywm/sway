#define _POSIX_C_SOURCE 200809L
#include "sway/config.h"

#include <stdbool.h>
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"

/**
 * Add gesture binding to config
 */
static struct cmd_results *bar_gesture_add(
		struct bar_gesture *binding, const char *name) {
	list_t *gestures = config->current_bar->gestures;

	// overwrite the binding if it already exists
	bool overwritten = false;
	for (int i = 0; i < gestures->length; ++i) {
		struct bar_gesture *current = gestures->items[i];
		if (gesture_equal(&current->gesture, &binding->gesture)) {

			overwritten = true;
			gestures->items[i] = binding;
			free_bar_gesture(current);
			sway_log(SWAY_DEBUG, "[bar %s] Updated gesture for %s",
					config->current_bar->id, name);
			break;
		}
	}

	if (!overwritten) {
		list_add(gestures, binding);
		sway_log(SWAY_DEBUG, "[bar %s] Added binding for %s",
				config->current_bar->id, name);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Remove gesture binding from config
 */
static struct cmd_results *bar_gesture_remove(
		struct bar_gesture *binding, const char *name) {
	list_t *gestures = config->current_bar->gestures;

	for (int i = 0; i < gestures->length; ++i) {
		struct bar_gesture *current = gestures->items[i];
		if (gesture_equal(&current->gesture, &binding->gesture)) {
			sway_log(SWAY_DEBUG, "[bar %s] Unbound binding for %s",
					config->current_bar->id, name);

			free_bar_gesture(current);
			free_bar_gesture(binding);
			list_del(gestures, i);

			return cmd_results_new(CMD_SUCCESS, NULL);
		}
	}

	free_bar_gesture(binding);
	return cmd_results_new(CMD_FAILURE,
			"Could not find gesture for [bar %s] %s",
			config->current_bar->id, name);
}

/**
 * Parse and execute bindgesture or unbindgesture command.
 */
static struct cmd_results *bar_cmd_gesture(int argc, char **argv, bool unbind) {
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

	struct bar_gesture *binding = calloc(1, sizeof(struct bar_gesture));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate binding");
	}
	char *errmsg = NULL;
	if ((errmsg = gesture_parse(argv[0], &binding->gesture))) {
		free(binding);
		struct cmd_results *final = cmd_results_new(
			CMD_FAILURE, "Invalid %s command (%s)", bindtype, errmsg);
		free(errmsg);
		return final;
	}

	if (unbind) {
		return bar_gesture_remove(binding, argv[0]);
	}
	binding->command = join_args(argv + 1, argc - 1);
	return bar_gesture_add(binding, argv[0]);
}

struct cmd_results *bar_cmd_bindgesture(int argc, char **argv) {
	return bar_cmd_gesture(argc, argv, false);
}

struct cmd_results *bar_cmd_unbindgesture(int argc, char **argv) {
	return bar_cmd_gesture(argc, argv, true);
}
