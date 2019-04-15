#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_new_float(int argc, char **argv) {
	sway_log(SWAY_INFO, "Warning: new_float is deprecated. "
		"Use default_floating_border instead.");
	if (config->reading) {
		config_add_swaynag_warning("new_float is deprecated. "
			"Use default_floating_border instead.");
	}
	return cmd_default_floating_border(argc, argv);
}
