#define _XOPEN_SOURCE 500
#include <signal.h>
#include "log.h"
#include "list.h"
#include "swaynag/config.h"
#include "swaynag/nagbar.h"
#include "swaynag/types.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static struct sway_nagbar nagbar;

void sig_handler(int signal) {
	nagbar_destroy(&nagbar);
	exit(EXIT_FAILURE);
}

void sway_terminate(int code) {
	nagbar_destroy(&nagbar);
	exit(code);
}

int main(int argc, char **argv) {
	int exit_code = EXIT_SUCCESS;

	list_t *types = create_list();
	nagbar_types_add_default(types);

	memset(&nagbar, 0, sizeof(nagbar));
	nagbar.anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	nagbar.font = strdup("pango:monospace 10");
	nagbar.buttons = create_list();

	struct sway_nagbar_button *button_close =
		calloc(sizeof(struct sway_nagbar_button), 1);
	button_close->text = strdup("X");
	button_close->type = NAGBAR_ACTION_DISMISS;
	list_add(nagbar.buttons, button_close);

	nagbar.details.button_details.text = strdup("Toggle Details");
	nagbar.details.button_details.type = NAGBAR_ACTION_EXPAND;


	char *config_path = NULL;
	bool debug = false;
	int launch_status = nagbar_parse_options(argc, argv, NULL, NULL,
			&config_path, &debug);
	if (launch_status != 0)  {
		exit_code = launch_status;
		goto cleanup;
	}
	wlr_log_init(debug ? WLR_DEBUG : WLR_ERROR, NULL);

	if (!config_path) {
		config_path = nagbar_get_config_path();
	}
	if (config_path) {
		wlr_log(WLR_DEBUG, "Loading config file: %s", config_path);
		int config_status = nagbar_load_config(config_path, &nagbar, types);
		free(config_path);
		if (config_status != 0) {
			exit_code = config_status;
			goto cleanup;
		}
	}

	if (argc > 1) {
		int result = nagbar_parse_options(argc, argv, &nagbar, types,
				NULL, NULL);
		if (result != 0) {
			exit_code = result;
			goto cleanup;
		}
	}

	if (!nagbar.message) {
		wlr_log(WLR_ERROR, "No message passed. Please provide --message/-m");
		exit_code = EXIT_FAILURE;
		goto cleanup;
	}

	if (!nagbar.type) {
		nagbar.type = nagbar_type_get(types, "error");
	}

	nagbar.type = nagbar_type_clone(nagbar.type);
	nagbar_types_free(types);

	if (nagbar.details.message) {
		list_add(nagbar.buttons, &nagbar.details.button_details);
	} else {
		free(nagbar.details.button_details.text);
	}

	wlr_log(WLR_DEBUG, "Output: %s", nagbar.output.name);
	wlr_log(WLR_DEBUG, "Anchors: %d", nagbar.anchors);
	wlr_log(WLR_DEBUG, "Type: %s", nagbar.type->name);
	wlr_log(WLR_DEBUG, "Message: %s", nagbar.message);
	wlr_log(WLR_DEBUG, "Font: %s", nagbar.font);
	wlr_log(WLR_DEBUG, "Buttons");
	for (int i = 0; i < nagbar.buttons->length; i++) {
		struct sway_nagbar_button *button = nagbar.buttons->items[i];
		wlr_log(WLR_DEBUG, "\t[%s] `%s`", button->text, button->action);
	}

	signal(SIGTERM, sig_handler);

	nagbar_setup(&nagbar);
	nagbar_run(&nagbar);
	return exit_code;

cleanup:
	nagbar_types_free(types);
	free(nagbar.details.button_details.text);
	nagbar_destroy(&nagbar);
	return exit_code;
}

