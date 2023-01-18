#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/container.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *cmd_shadows(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "shadows", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	struct sway_container *con = config->handler_context.container;

	bool result = parse_boolean(argv[0], config->shadow_enabled);
	if (con == NULL) {
		config->shadow_enabled = result;
	} else {
		con->shadow_enabled = result;
		container_damage_whole(con);
	}

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
