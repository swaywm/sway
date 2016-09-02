#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_tray_output(int argc, char **argv) {
	sway_log(L_ERROR, "Warning: tray_output is not supported on wayland");
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
