#define _POSIX_C_SOURCE 200809L
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
	wl_list_init(&swaynag.outputs);
	wl_list_init(&swaynag.seats);

	struct swaynag_button *button_close =
		calloc(sizeof(struct swaynag_button), 1);
	button_close->text = strdup("X");
	button_close->type = SWAYNAG_ACTION_DISMISS;
	list_add(swaynag.buttons, button_close);

	swaynag.details.button_details =
		calloc(sizeof(struct swaynag_button), 1);
	swaynag.details.button_details->text = strdup("Toggle details");
	swaynag.details.button_details->type = SWAYNAG_ACTION_EXPAND;

	char *config_path = NULL;
	bool debug = false;
	int launch_status = swaynag_parse_options(argc, argv, NULL, NULL, NULL,
			&config_path, &debug);
	if (launch_status != 0)  {
		exit_code = launch_status;
		goto cleanup;
	}
	sway_log_init(debug ? SWAY_DEBUG : SWAY_ERROR, NULL);

	if (!config_path) {
		config_path = swaynag_get_config_path();
	}
	if (config_path) {
		sway_log(SWAY_DEBUG, "Loading config file: %s", config_path);
		int config_status = swaynag_load_config(config_path, &swaynag, types);
		free(config_path);
		if (config_status != 0) {
			exit_code = config_status;
			goto cleanup;
		}
	}

	if (argc > 1) {
		struct swaynag_type *type_args = swaynag_type_new("<args>");
		list_add(types, type_args);

		int result = swaynag_parse_options(argc, argv, &swaynag, types,
				type_args, NULL, NULL);
		if (result != 0) {
			exit_code = result;
			goto cleanup;
		}
	}

	if (!swaynag.message) {
		sway_log(SWAY_ERROR, "No message passed. Please provide --message/-m");
		exit_code = EXIT_FAILURE;
		goto cleanup;
	}

	if (!swaynag.type) {
		swaynag.type = swaynag_type_get(types, "error");
	}

	// Construct a new type with the defaults as the base, the general config
	// on top of that, followed by the type config, and finally any command
	// line arguments
	struct swaynag_type *type = swaynag_type_new(swaynag.type->name);
	swaynag_type_merge(type, swaynag_type_get(types, "<defaults>"));
	swaynag_type_merge(type, swaynag_type_get(types, "<config>"));
	swaynag_type_merge(type, swaynag.type);
	swaynag_type_merge(type, swaynag_type_get(types, "<args>"));
	swaynag.type = type;

	swaynag_types_free(types);

	if (swaynag.details.message) {
		list_add(swaynag.buttons, swaynag.details.button_details);
	} else {
		free(swaynag.details.button_details->text);
		free(swaynag.details.button_details);
	}

	sway_log(SWAY_DEBUG, "Output: %s", swaynag.type->output);
	sway_log(SWAY_DEBUG, "Anchors: %" PRIu32, swaynag.type->anchors);
	sway_log(SWAY_DEBUG, "Type: %s", swaynag.type->name);
	sway_log(SWAY_DEBUG, "Message: %s", swaynag.message);
	sway_log(SWAY_DEBUG, "Font: %s", swaynag.type->font);
	sway_log(SWAY_DEBUG, "Buttons");
	for (int i = 0; i < swaynag.buttons->length; i++) {
		struct swaynag_button *button = swaynag.buttons->items[i];
		sway_log(SWAY_DEBUG, "\t[%s] `%s`", button->text, button->action);
	}

	signal(SIGTERM, sig_handler);

	swaynag_setup(&swaynag);
	swaynag_run(&swaynag);
	return exit_code;

cleanup:
	swaynag_types_free(types);
	free(swaynag.details.button_details->text);
	free(swaynag.details.button_details);
	swaynag_destroy(&swaynag);
	return exit_code;
}
