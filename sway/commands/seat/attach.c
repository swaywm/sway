#define _XOPEN_SOURCE 700
#include <string.h>
#include <strings.h>
#include "sway/input/input-manager.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *seat_cmd_attach(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "attach", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_seat_config) {
		return cmd_results_new(CMD_FAILURE, "attach", "No seat defined");
	}

	struct seat_config *new_config = new_seat_config(current_seat_config->name);
	struct seat_attachment_config *new_attachment = seat_attachment_config_new();
	new_attachment->identifier = strdup(argv[0]);
	list_add(new_config->attachments, new_attachment);

	apply_seat_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
