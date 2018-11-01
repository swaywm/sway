#define _XOPEN_SOURCE 700
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"

struct cmd_results *seat_cmd_detach(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "detach", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct seat_config *current_seat_config = config->handler_context.seat_config;
	if (!current_seat_config) {
		return cmd_results_new(CMD_FAILURE, "detach", "No seat defined");
	}

	int i;
	i = list_seq_find(config->seat_configs, seat_name_cmp, current_seat_config->name);
	if (i >= 0) {
		struct seat_config *sc = config->seat_configs->items[i];
		seat_config_remove_attachment(sc, strdup(argv[0]));
	}

	if (!config->validating) {
		input_manager_apply_seat_config(new_seat_config(current_seat_config->name));
	}

	input_manager_seat_consider_destroy(current_seat_config->name);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
