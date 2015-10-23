#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include <json-c/json.h>
#include "config.h"


enum  cmd_status {
	CMD_SUCCESS,
	CMD_FAILURE, // was or at least could be executed
	CMD_INVALID, // unknown or parse error
	CMD_DEFER,
	// Config Blocks
	CMD_BLOCK_END,
	CMD_BLOCK_MODE,
};

struct cmd_results {
	enum cmd_status status;
	char *input;
	char *error;
};

struct cmd_results *handle_command(char *command);
// Handles commands during config
struct cmd_results *config_command(char *command);

struct cmd_results *cmd_results_new(enum cmd_status status, const char* input, const char *error, ...);
void free_cmd_results(struct cmd_results *results);
const char *cmd_results_to_json(struct cmd_results *results);

void remove_view_from_scratchpad();

#endif
