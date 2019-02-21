#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "log.h"

struct cmd_results *cmd_titlebar_border_thickness(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "titlebar_border_thickness", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *inv;
	int value = strtol(argv[0], &inv, 10);
	if (*inv != '\0' || value < 0 || value > config->titlebar_v_padding) {
		return cmd_results_new(CMD_FAILURE, "Invalid size specified");
	}

	config->titlebar_border_thickness = value;

	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		struct sway_workspace *ws = output_get_active_workspace(output);
		if (!sway_assert(ws, "Expected output to have a workspace")) {
			return cmd_results_new(CMD_FAILURE,
					"Expected output to have a workspace");
		}
		arrange_workspace(ws);
		output_damage_whole(output);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
