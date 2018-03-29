#include <stdlib.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_context_button(int argc, char **argv) {
	const char *cmd_name = "context_button";
	// TODO TRAY
	return cmd_results_new(CMD_INVALID, cmd_name, "TODO TRAY");
}
