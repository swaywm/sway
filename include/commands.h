#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include "config.h"


enum  cmd_status {
	CMD_SUCCESS,
	CMD_FAILURE,
	CMD_INVALID,
	CMD_DEFER,
	// Config Blocks
	CMD_BLOCK_END,
	CMD_BLOCK_MODE,
};

enum cmd_status handle_command(char *command);
// Handles commands during config
enum cmd_status config_command(char *command);

void remove_view_from_scratchpad();

#endif
