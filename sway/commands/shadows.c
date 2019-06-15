#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
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


static struct cmd_results *handle_shadow_cmd(int argc, char **argv, struct shadow_config* class, const char* cmd_name) {

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

  class->radius = atoi(argv[0]);
  class->offset = atoi(argv[1]);

	if (!parse_color_float(argv[2], class->color)) {
		return cmd_results_new(CMD_INVALID,
				"Unable to parse shadow color '%s'", argv[0]);
	}

	if (config->active) {
		for (int i = 0; i < root->outputs->length; ++i) {
			struct sway_output *output = root->outputs->items[i];
			output_damage_whole(output);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_shadows_focused(int argc, char **argv) {
  return handle_shadow_cmd(argc, argv, &config->shadow_config.focused, "shadows.focused");
}

struct cmd_results *cmd_shadows_focused_inactive(int argc, char **argv) {
  return handle_shadow_cmd(argc, argv, &config->shadow_config.focused_inactive, "shadows.focused_inactive");
}

struct cmd_results *cmd_shadows_unfocused(int argc, char **argv) {
  return handle_shadow_cmd(argc, argv, &config->shadow_config.unfocused, "shadows.unfocused");
}

struct cmd_results *cmd_shadows_urgent(int argc, char **argv) {
  return handle_shadow_cmd(argc, argv, &config->shadow_config.urgent, "shadows.urgent");
}
