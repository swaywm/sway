#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *cmd_backend(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "backend", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	int argc_new = argc-1;
	char **argv_new = argv+1;

	struct cmd_results *res;
	if (strcasecmp("add", argv[0]) == 0) {
		res = backend_cmd_add(argc_new, argv_new);
	} else if (strcasecmp("del", argv[0]) == 0) {
		res = backend_cmd_del(argc_new, argv_new);
	} else {
		res = cmd_results_new(CMD_INVALID, "backend <cmd> [args]", "Unknown command %s", argv[1]);
	}
	return res;
}
