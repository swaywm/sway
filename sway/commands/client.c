#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "util.h"

static void rebuild_textures_iterator(struct sway_container *con, void *data) {
	container_update_marks_textures(con);
	container_update_title_textures(con);
}

static struct cmd_results *handle_command(int argc, char **argv,
		struct border_colors *class, char *cmd_name) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 5))) {
		return error;
	}

	struct border_colors colors = {0};

	struct {
		const char *name;
		float *rgba[4];
	} properties[] = {
		{ "border", colors.border },
		{ "background", colors.background },
		{ "text", colors.text },
		{ "indicator", colors.indicator },
		{ "child_border", colors.child_border }
	};
	for (size_t i = 0; i < sizeof(properties) / sizeof(properties[0]); i++) {
		uint32_t color;
		if (!parse_color(argv[i], &color)) {
			return cmd_results_new(CMD_INVALID,
					"Invalid %s color %s", properties[i].name, argv[i]);
		}
		color_to_rgba(*properties[i].rgba, color);
	}

	memcpy(class, &colors, sizeof(struct border_colors));

	if (config->active) {
		root_for_each_container(rebuild_textures_iterator, NULL);

		for (int i = 0; i < root->outputs->length; ++i) {
			struct sway_output *output = root->outputs->items[i];
			output_damage_whole(output);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
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

struct cmd_results *cmd_client_noop(int argc, char **argv) {
	sway_log(SWAY_INFO, "Warning: %s is ignored by sway", argv[-1]);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
