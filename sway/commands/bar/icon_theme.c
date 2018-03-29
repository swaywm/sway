#define _XOPEN_SOURCE 500
#include <string.h>
#include "sway/commands.h"

struct cmd_results *bar_cmd_icon_theme(int argc, char **argv) {
	const char *cmd_name = "tray_output";
	// TODO TRAY
	return cmd_results_new(CMD_INVALID, cmd_name, "TODO TRAY");
}
