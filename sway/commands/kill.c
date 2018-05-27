#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/view.h"
#include "sway/tree/container.h"
#include "sway/commands.h"

struct cmd_results *cmd_kill(int argc, char **argv) {
	struct sway_container *con =
		config->handler_context.current_container;

	container_close(con);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
