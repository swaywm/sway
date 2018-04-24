#include <wlr/util/log.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/idle.h"

struct cmd_results *cmd_lock(int argc, char **argv) {

	invoke_swaylock();

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
