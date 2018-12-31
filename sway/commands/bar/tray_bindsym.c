#include <strings.h>
#include "config.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"

struct cmd_results *bar_cmd_tray_bindsym(int argc, char **argv) {
#if HAVE_TRAY
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tray_bindsym", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "tray_bindsym", "No bar defined.");
	}

	int button = 0;
	if (strncasecmp(argv[0], "button", strlen("button")) == 0 &&
			strlen(argv[0]) == strlen("button0")) {
		button = argv[0][strlen("button")] - '0';
	}
	if (button < 1 || button > 9) {
		return cmd_results_new(CMD_FAILURE, "tray_bindsym",
				"[Bar %s] Only buttons 1 to 9 are supported",
				config->current_bar->id);
	}

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
			wlr_log(WLR_DEBUG, "[Bar %s] Binding button %d to %s",
					config->current_bar->id, button, commands[i]);
			config->current_bar->tray_bindings[button] = commands[i];
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}
	}

	return cmd_results_new(CMD_INVALID, "tray_bindsym",
			"[Bar %s] Invalid command %s", config->current_bar->id, argv[1]);
#else
	return cmd_results_new(CMD_INVALID, "tray_bindsym",
			"Sway has been compiled without tray support");
#endif
}
