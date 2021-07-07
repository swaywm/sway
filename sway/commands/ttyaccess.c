#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/container.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *cmd_no_titlebars(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "ttyaccess", EXPECTED_EQUAL_TO, 1);

	if (error) {
		return error;
	}

	config->ttyaccess = parse_boolean(argv[0], config->ttyaccess);

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
