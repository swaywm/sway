#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *bar_cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bar bindsym", EXPECTED_AT_LEAST, 2))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "bar bindsym", "No bar defined.");
	}

	struct bar_binding *binding = calloc(1, sizeof(struct bar_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "bar bindsym",
				"Unable to allocate bar binding");
	}

	binding->release = false;
	if (strcmp("--release", argv[0]) == 0) {
		binding->release = true;
		argv++;
		argc--;
	}

	binding->button = 0;
	if (strncasecmp(argv[0], "button", strlen("button")) == 0 &&
			strlen(argv[0]) == strlen("button0")) {
		binding->button = argv[0][strlen("button")] - '0';
	}
	if (binding->button < 1 || binding->button > 9) {
		free_bar_binding(binding);
		return cmd_results_new(CMD_FAILURE, "bar bindsym",
				"Only button<1-9> is supported");
	}

	binding->command = join_args(argv + 1, argc - 1);

	list_t *bindings = config->current_bar->bindings;
	bool overwritten = false;
	for (int i = 0; i < bindings->length; i++) {
		struct bar_binding *other = bindings->items[i];
		if (other->button == binding->button &&
				other->release == binding->release) {
			overwritten = true;
			bindings->items[i] = binding;
			free_bar_binding(other);
			wlr_log(WLR_DEBUG, "[bar %s] Updated binding for button%u%s",
					config->current_bar->id, binding->button,
					binding->release ? " (release)" : "");
			break;
		}
	}
	if (!overwritten) {
		list_add(bindings, binding);
		wlr_log(WLR_DEBUG, "[bar %s] Added binding for button%u%s",
				config->current_bar->id, binding->button,
				binding->release ? " (release)" : "");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
