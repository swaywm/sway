#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include "config.h"

struct cmd_handler {
	char *command;
	bool (*handle)(struct sway_config *config, int argc, char **argv);
};

int handle_command(struct sway_config *config, char *command);

#endif
