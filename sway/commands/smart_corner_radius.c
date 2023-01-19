#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "util.h"

struct cmd_results *cmd_smart_corner_radius(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "smart_corner_radius", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->smart_corner_radius = parse_boolean(argv[0], true);

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
