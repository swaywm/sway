#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include "config.h"

struct cmd_handler {
	char *command;
	bool (*handle)(struct sway_config *config, int argc, char **argv);
	// if <0 command is deffered until compositor is ready.
	// if =0 command can be called anytime.
	// if >0 command can only be called via keybind, ignored in config
	int config_type;
};

struct cmd_handler *find_handler(char *line);
bool handle_command(struct sway_config *config, char *command);

void remove_view_from_scratchpad();

#endif
