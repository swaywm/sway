#include "log.h"
#include "sway/commands.h"

struct cmd_results *cmd_new_float(int argc, char **argv) {
	sway_log(L_INFO, "`new_float` is deprecated and will be removed in the future. "
		"Please use `default_floating_border` instead.");
	return cmd_default_floating_border(argc, argv);
}
