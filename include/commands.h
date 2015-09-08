#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include "config.h"

struct cmd_handler {
	char *command;
	bool (*handle)(int argc, char **argv);
	enum {
		CMD_COMPOSITOR_READY,
		CMD_KEYBIND,
		CMD_ANYTIME
	} config_type;
};

struct cmd_handler *find_handler(char *line);
bool handle_command(char *command);
// Handles commands during config
bool config_command(char *command);

void remove_view_from_scratchpad();

#endif
