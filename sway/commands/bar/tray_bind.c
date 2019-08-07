#include <strings.h>
#include "config.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "log.h"

static struct cmd_results *tray_bind(int argc, char **argv, bool code) {
#if HAVE_TRAY
	const char *command = code ? "bar tray_bindcode" : "bar tray_bindsym";
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, command, EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	struct tray_binding *binding = calloc(1, sizeof(struct tray_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate tray binding");
	}

	char *message = NULL;
	if (code) {
		binding->button = get_mouse_bindcode(argv[0], &message);
	} else {
		binding->button = get_mouse_bindsym(argv[0], &message);
	}
	if (message) {
		free(binding);
		error = cmd_results_new(CMD_INVALID, message);
		free(message);
		return error;
	} else if (!binding->button) {
		free(binding);
		return cmd_results_new(CMD_INVALID, "Unknown button %s", argv[0]);
	}
	const char *name = get_mouse_button_name(binding->button);

	static const char *commands[] = {
		"ContextMenu",
		"Activate",
		"SecondaryActivate",
		"ScrollDown",
		"ScrollLeft",
		"ScrollRight",
		"ScrollUp",
		"nop"
	};

	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
		if (strcasecmp(argv[1], commands[i]) == 0) {
			binding->command = commands[i];
		}
	}
	if (!binding->command) {
		free(binding);
		return cmd_results_new(CMD_INVALID, "[Bar %s] Invalid tray command %s",
				config->current_bar->id, argv[1]);
	}

	bool overwritten = false;
	struct tray_binding *other = NULL;
	wl_list_for_each(other, &config->current_bar->tray_bindings, link) {
		if (other->button == binding->button) {
			overwritten = true;
			other->command = binding->command;
			free(binding);
			binding = other;
			sway_log(SWAY_DEBUG,
					"[bar %s] Updated tray binding for %u (%s) to %s",
					config->current_bar->id, binding->button, name,
					binding->command);
			break;
		}
	}
	if (!overwritten) {
		wl_list_insert(&config->current_bar->tray_bindings, &binding->link);
		sway_log(SWAY_DEBUG, "[bar %s] Added tray binding for %u (%s) to %s",
				config->current_bar->id, binding->button, name,
				binding->command);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
#else
	return cmd_results_new(CMD_INVALID,
			"Sway has been compiled without tray support");
#endif
}

struct cmd_results *bar_cmd_tray_bindcode(int argc, char **argv) {
	return tray_bind(argc, argv, true);
}

struct cmd_results *bar_cmd_tray_bindsym(int argc, char **argv) {
	return tray_bind(argc, argv, false);
}
