#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include "config.h"

typedef enum  cmd_status {
	CMD_SUCCESS,
	CMD_FAILURE,
	CMD_DEFER,
} sway_cmd(char *criteria, int argc, char **argv);

struct cmd_handler {
	const char*command;
	sway_cmd  *handle;
};

enum cmd_status handle_command(char *command);
// Handles commands during config
enum cmd_status config_command(char *command);

void remove_view_from_scratchpad(swayc_t *view);

#endif
