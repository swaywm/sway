#include "sway/commands.h"

struct cmd_results *cmd_nop(int argc, char **argv) {
	return cmd_results_new(CMD_SUCCESS, NULL);
}
