#ifndef _SWAY_COMMANDS_H
#define _SWAY_COMMANDS_H
#include <stdbool.h>
#include <json-c/json.h>
#include <wlc/wlc.h>
#include "config.h"

/**
 * Indicates the result of a command's execution.
 */
enum cmd_status {
	CMD_SUCCESS, 		/**< The command was successful */
	CMD_FAILURE,		/**< The command resulted in an error */
	CMD_INVALID, 		/**< Unknown command or parser error */
	CMD_DEFER,			/**< Command execution deferred */
	// Config Blocks
	CMD_BLOCK_END,
	CMD_BLOCK_MODE,
	CMD_BLOCK_BAR,
	CMD_BLOCK_BAR_COLORS,
	CMD_BLOCK_INPUT
};

/**
 * Stores the result of executing a command.
 */
struct cmd_results {
	enum cmd_status status;
	char *input;
	/**
	 * Human friendly error message, or NULL on success
	 */
	char *error;
};

/**
 * Parse and handles a command.
 */
struct cmd_results *handle_command(char *command);
/**
 * Parse and handles a command during config file loading.
 *
 * Do not use this under normal conditions.
 */
struct cmd_results *config_command(char *command, enum cmd_status block);

/**
 * Allocates a cmd_results object.
 */
struct cmd_results *cmd_results_new(enum cmd_status status, const char* input, const char *error, ...);
/**
 * Frees a cmd_results object.
 */
void free_cmd_results(struct cmd_results *results);
/**
 * Serializes cmd_results to a JSON string.
 *
 * Free the JSON string later on.
 */
const char *cmd_results_to_json(struct cmd_results *results);

void remove_view_from_scratchpad();

#endif
