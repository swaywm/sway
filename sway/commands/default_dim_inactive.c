#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "sway/output.h"


struct cmd_results *cmd_default_dim_inactive(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "default_dim_inactive", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *err;
	float val = strtof(argv[0], &err);
	if (*err || val < 0.0f || val > 1.0f) {
		return cmd_results_new(CMD_INVALID, "default_dim_inactive float invalid");
	}

	config->default_dim_inactive = val;

	if (config->active) {
		for (int i = 0; i < root->outputs->length; ++i) {
			struct sway_output *output = root->outputs->items[i];
			output_damage_whole(output);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
