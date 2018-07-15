#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/container.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_smart_gaps(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "smart_gaps", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	if (strcmp(argv[0], "on") == 0) {
		config->smart_gaps = true;
	} else if (strcmp(argv[0], "off") == 0) {
		config->smart_gaps = false;
	} else {
		return cmd_results_new(CMD_INVALID, "smart_gaps",
			"Expected 'smart_gaps <on|off>' ");
	}

	arrange_windows(&root_container);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
