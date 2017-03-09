#include "log.h"
#include "sway/commands.h"

struct cmd_results *cmd_new_window(int argc, char **argv) {
	sway_log(L_INFO, "`new_window` is deprecated and will be removed in the future. "
		"Please use `default_border` instead.");
	return cmd_default_border(argc, argv);
}
