#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/container.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *cmd_shadows_on_csd(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "shadows_on_csd", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	config->shadows_on_csd_enabled = parse_boolean(argv[0], config->shadows_on_csd_enabled);

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
