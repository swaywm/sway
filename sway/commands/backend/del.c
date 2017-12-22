#define _XOPEN_SOURCE 700
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/server.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *backend_cmd_del(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "del", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	sway_server_remove_subbackend(&server, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
