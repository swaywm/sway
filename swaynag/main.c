#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <signal.h>
#include "log.h"
#include "list.h"
#include "swaynag/config.h"
#include "swaynag/swaynag.h"
#include "swaynag/types.h"

static struct swaynag swaynag;

void sig_handler(int signal) {
	swaynag_destroy(&swaynag);
	exit(EXIT_FAILURE);
}

void sway_terminate(int code) {
	swaynag_destroy(&swaynag);
	exit(code);
}

int main(int argc, char **argv) {
	int exit_code = EXIT_SUCCESS;

	list_t *types = create_list();
	swaynag_types_add_default(types);

	memset(&swaynag, 0, sizeof(swaynag));
	swaynag.buttons = create_list();

	struct swaynag_button *button_close =
		calloc(sizeof(struct swaynag_button), 1);
	button_close->text = strdup("X");
	button_close->type = SWAYNAG_ACTION_DISMISS;
	list_add(swaynag.buttons, button_close);

	swaynag.details.button_details.text = strdup("Toggle Details");
	swaynag.details.button_details.type = SWAYNAG_ACTION_EXPAND;


	char *config_path = NULL;
	bool debug = false;
	int launch_status = swaynag_parse_options(argc, argv, NULL, NULL, NULL,
			&config_path, &debug);
	if (launch_status != 0)  {
		exit_code = launch_status;
		goto cleanup;
	}
	wlr_log_init(debug ? WLR_DEBUG : WLR_ERROR, NULL);

	if (!config_path) {
		config_path = swaynag_get_config_path();
	}
	if (config_path) {
		wlr_log(WLR_DEBUG, "Loading config file: %s", config_path);
		int config_status = swaynag_load_config(config_path, &swaynag, types);
		free(config_path);
		if (config_status != 0) {
			exit_code = config_status;
			goto cleanup;
		}
	}

	if (argc > 1) {
		struct swaynag_type *type_args;
		type_args = calloc(1, sizeof(struct swaynag_type));
		type_args->name = strdup("<args>");
		list_add(types, type_args);

		int result = swaynag_parse_options(argc, argv, &swaynag, types,
				type_args, NULL, NULL);
		if (result != 0) {
			exit_code = result;
			goto cleanup;
		}
	}

	if (!swaynag.message) {
		wlr_log(WLR_ERROR, "No message passed. Please provide --message/-m");
		exit_code = EXIT_FAILURE;
		goto cleanup;
	}

	if (!swaynag.type) {
		swaynag.type = swaynag_type_get(types, "error");
	}

	// Construct a new type using the config defaults as base, then merging
	// config type defaults on top, then merging arguments on top of that, and
	// finally merging defaults on top.
	struct swaynag_type *type = calloc(1, sizeof(struct swaynag_type));
	type->name = strdup(swaynag.type->name);
	swaynag_type_merge(type, swaynag_type_get(types, "<args>"));
	swaynag_type_merge(type, swaynag.type);
	swaynag_type_merge(type, swaynag_type_get(types, "<config>"));
	swaynag_type_merge(type, swaynag_type_get(types, "<defaults>"));
	swaynag.type = type;

	swaynag_types_free(types);

	if (swaynag.details.message) {
		list_add(swaynag.buttons, &swaynag.details.button_details);
	} else {
		free(swaynag.details.button_details.text);
	}

	wlr_log(WLR_DEBUG, "Output: %s", swaynag.type->output);
	wlr_log(WLR_DEBUG, "Anchors: %d", swaynag.type->anchors);
	wlr_log(WLR_DEBUG, "Type: %s", swaynag.type->name);
	wlr_log(WLR_DEBUG, "Message: %s", swaynag.message);
	wlr_log(WLR_DEBUG, "Font: %s", swaynag.type->font);
	wlr_log(WLR_DEBUG, "Buttons");
	for (int i = 0; i < swaynag.buttons->length; i++) {
		struct swaynag_button *button = swaynag.buttons->items[i];
		wlr_log(WLR_DEBUG, "\t[%s] `%s`", button->text, button->action);
	}

	signal(SIGTERM, sig_handler);

	swaynag_setup(&swaynag);
	swaynag_run(&swaynag);
	return exit_code;

cleanup:
	swaynag_types_free(types);
	free(swaynag.details.button_details.text);
	swaynag_destroy(&swaynag);
	return exit_code;
}

