#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/container.h"

/**
 * Parse the hex string into an integer.
 */
static bool parse_color_int(char *hexstring, uint32_t *dest) {
	if (hexstring[0] != '#') {
		return false;
	}

	if (strlen(hexstring) != 7 && strlen(hexstring) != 9) {
		return false;
	}

	++hexstring;
	char *end;
	uint32_t decimal = strtol(hexstring, &end, 16);

	if (*end != '\0') {
		return false;
	}

	if (strlen(hexstring) == 6) {
		// Add alpha
		decimal = (decimal << 8) | 0xff;
	}

	*dest = decimal;
	return true;
}

/**
 * Parse the hex string into a float value.
 */
static bool parse_color_float(char *hexstring, float dest[static 4]) {
	uint32_t decimal;
	if (!parse_color_int(hexstring, &decimal)) {
		return false;
	}
	dest[0] = ((decimal >> 24) & 0xff) / 255.0;
	dest[1] = ((decimal >> 16) & 0xff) / 255.0;
	dest[2] = ((decimal >> 8) & 0xff) / 255.0;
	dest[3] = (decimal & 0xff) / 255.0;
	return true;
}

static struct cmd_results *handle_command(int argc, char **argv,
		struct border_colors *class, char *cmd_name) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 5))) {
		return error;
	}

	if (!parse_color_float(argv[0], class->border)) {
		return cmd_results_new(CMD_INVALID, cmd_name,
				"Unable to parse border color");
	}

	if (!parse_color_float(argv[1], class->background)) {
		return cmd_results_new(CMD_INVALID, cmd_name,
				"Unable to parse background color");
	}

	if (!parse_color_int(argv[2], &class->text)) {
		return cmd_results_new(CMD_INVALID, cmd_name,
				"Unable to parse text color");
	}

	if (!parse_color_float(argv[3], class->indicator)) {
		return cmd_results_new(CMD_INVALID, cmd_name,
				"Unable to parse indicator color");
	}

	if (!parse_color_float(argv[4], class->child_border)) {
		return cmd_results_new(CMD_INVALID, cmd_name,
				"Unable to parse child border color");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_client_focused(int argc, char **argv) {
	return handle_command(argc, argv, &config->border_colors.focused, "client.focused");
}

struct cmd_results *cmd_client_focused_inactive(int argc, char **argv) {
	return handle_command(argc, argv, &config->border_colors.focused_inactive, "client.focused_inactive");
}

struct cmd_results *cmd_client_unfocused(int argc, char **argv) {
	return handle_command(argc, argv, &config->border_colors.unfocused, "client.unfocused");
}

struct cmd_results *cmd_client_urgent(int argc, char **argv) {
	return handle_command(argc, argv, &config->border_colors.urgent, "client.urgent");
}
