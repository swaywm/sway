#include "log.h"
#include "sway/commands.h"

struct cmd_results *output_cmd_dpms(int argc, char **argv) {
	sway_log(SWAY_INFO, "The \"output dpms\" command is deprecated, "
		"use \"output power\" instead");
	return output_cmd_power(argc, argv);
}
