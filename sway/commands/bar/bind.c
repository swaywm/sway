#include <libevdev/libevdev.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

static struct cmd_results *binding_add(struct bar_binding *binding,
		list_t *mode_bindings) {
	const char *name = get_mouse_button_name(binding->button);
	bool overwritten = false;
	for (int i = 0; i < mode_bindings->length; i++) {
		struct bar_binding *other = mode_bindings->items[i];
		if (other->button == binding->button &&
				other->release == binding->release) {
			overwritten = true;
			mode_bindings->items[i] = binding;
			free_bar_binding(other);
			sway_log(SWAY_DEBUG, "[bar %s] Updated binding for %u (%s)%s",
					config->current_bar->id, binding->button, name,
					binding->release ? " - release" : "");
			break;
		}
	}
	if (!overwritten) {
		list_add(mode_bindings, binding);
		sway_log(SWAY_DEBUG, "[bar %s] Added binding for %u (%s)%s",
				config->current_bar->id, binding->button, name,
				binding->release ? " - release" : "");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *binding_remove(struct bar_binding *binding,
		list_t *mode_bindings) {
	const char *name = get_mouse_button_name(binding->button);
	for (int i = 0; i < mode_bindings->length; i++) {
		struct bar_binding *other = mode_bindings->items[i];
		if (other->button == binding->button &&
				other->release == binding->release) {
			sway_log(SWAY_DEBUG, "[bar %s] Unbound binding for %u (%s)%s",
					config->current_bar->id, binding->button, name,
					binding->release ? " - release" : "");
			free_bar_binding(other);
			free_bar_binding(binding);
			list_del(mode_bindings, i);
			return cmd_results_new(CMD_SUCCESS, NULL);
		}
	}

	struct cmd_results *error = cmd_results_new(CMD_FAILURE, "Could not "
			"find binding for [bar %s]" " Button %u (%s)%s",
			config->current_bar->id, binding->button, name,
			binding->release ? " - release" : "");
	free_bar_binding(binding);
	return error;
}

static struct cmd_results *bar_cmd_bind(int argc, char **argv, bool code,
		bool unbind) {
	int minargs = 2;
	const char *command;
	if (unbind) {
		minargs--;
		command = code ? "bar unbindcode" : "bar unbindsym";
	} else {
		command = code ? "bar bindcode" : "bar bindsym";
	}

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, command, EXPECTED_AT_LEAST, minargs))) {
		return error;
	}

	struct bar_binding *binding = calloc(1, sizeof(struct bar_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate bar binding");
	}

	binding->release = false;
	if (strcmp("--release", argv[0]) == 0) {
		binding->release = true;
		argv++;
		argc--;
	}

	char *message = NULL;
	if (code) {
		binding->button = get_mouse_bindcode(argv[0], &message);
	} else {
		binding->button = get_mouse_bindsym(argv[0], &message);
	}
	if (message) {
		free_bar_binding(binding);
		error = cmd_results_new(CMD_INVALID, message);
		free(message);
		return error;
	} else if (!binding->button) {
		free_bar_binding(binding);
		return cmd_results_new(CMD_INVALID, "Unknown button %s", argv[0]);
	}
	list_t *bindings = config->current_bar->bindings;
	if (unbind) {
		return binding_remove(binding, bindings);
	}

	binding->command = join_args(argv + 1, argc - 1);
	return binding_add(binding, bindings);
}

struct cmd_results *bar_cmd_bindcode(int argc, char **argv) {
	return bar_cmd_bind(argc, argv, true, false);
}

struct cmd_results *bar_cmd_bindsym(int argc, char **argv) {
	return bar_cmd_bind(argc, argv, false, false);
}

struct cmd_results *bar_cmd_unbindcode(int argc, char **argv) {
	return bar_cmd_bind(argc, argv, true, true);
}

struct cmd_results *bar_cmd_unbindsym(int argc, char **argv) {
	return bar_cmd_bind(argc, argv, false, true);
}
