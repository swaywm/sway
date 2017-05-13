#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *bar_cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1))) {
		return error;
	} else if (!config->reading) {
		return cmd_results_new(CMD_FAILURE, "bindsym", "Can only be used in config file.");
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "bindsym", "No bar defined.");
	}

	if (strlen(argv[1]) != 7) {
		return cmd_results_new(CMD_INVALID, "bindsym", "Invalid mouse binding %s", argv[1]);
	}
	uint32_t numbutton = (uint32_t)atoi(argv[1] + 6);
	if (numbutton < 1 || numbutton > 5 || strncmp(argv[1], "button", 6) != 0) {
		return cmd_results_new(CMD_INVALID, "bindsym", "Invalid mouse binding %s", argv[1]);
	}
	struct sway_mouse_binding *binding = malloc(sizeof(struct sway_mouse_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "bindsym", "Unable to allocate binding");
	}
	binding->button = numbutton;
	binding->command = join_args(argv + 1, argc - 1);

	struct bar_config *bar = config->current_bar;
	struct sway_mouse_binding *dup;
	ssize_t i = list_lsearch(bar->bindings, sway_mouse_binding_cmp_buttons, &binding, &dup);
	if (i > -1) {
		sway_log(L_DEBUG, "bindsym - '%s' for swaybar already exists, overwriting", argv[0]);
		free_sway_mouse_binding(dup);
		list_delete(bar->bindings, i);
	}
	list_add(bar->bindings, &binding);
	list_qsort(bar->bindings, sway_mouse_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s when clicking swaybar", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
