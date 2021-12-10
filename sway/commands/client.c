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

static struct cmd_results *handle_command(int argc, char **argv, char *cmd_name,
		struct border_colors *class, const char *default_indicator) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_AT_LEAST, 3)) ||
			(error = checkarg(argc, cmd_name, EXPECTED_AT_MOST, 5))) {
		return error;
	}

	if (argc > 3 && strcmp(cmd_name, "client.focused_tab_title") == 0) {
		sway_log(SWAY_ERROR,
				"Warning: indicator and child_border colors have no effect for %s",
				cmd_name);
	}

	struct border_colors colors = {0};
	const char *ind_hex = argc > 3 ? argv[3] : default_indicator;
	const char *child_hex = argc > 4 ? argv[4] : argv[1];  // def to background

	struct {
		const char *name;
		const char *hex;
		float *rgba[4];
	} properties[] = {
		{ "border", argv[0], colors.border },
		{ "background", argv[1], colors.background },
		{ "text", argv[2], colors.text },
		{ "indicator", ind_hex, colors.indicator },
		{ "child_border", child_hex, colors.child_border }
	};
	for (size_t i = 0; i < sizeof(properties) / sizeof(properties[0]); i++) {
		uint32_t color;
		if (!parse_color(properties[i].hex, &color)) {
			return cmd_results_new(CMD_INVALID, "Invalid %s color %s",
					properties[i].name, properties[i].hex);
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
	return handle_command(argc, argv, "client.focused",
			&config->border_colors.focused, "#2e9ef4ff");
}

struct cmd_results *cmd_client_focused_inactive(int argc, char **argv) {
	return handle_command(argc, argv, "client.focused_inactive",
			&config->border_colors.focused_inactive, "#484e50ff");
}

struct cmd_results *cmd_client_unfocused(int argc, char **argv) {
	return handle_command(argc, argv, "client.unfocused",
			&config->border_colors.unfocused, "#292d2eff");
}

struct cmd_results *cmd_client_urgent(int argc, char **argv) {
	return handle_command(argc, argv, "client.urgent",
			&config->border_colors.urgent, "#900000ff");
}

struct cmd_results *cmd_client_noop(int argc, char **argv) {
	sway_log(SWAY_INFO, "Warning: %s is ignored by sway", argv[-1]);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_client_focused_tab_title(int argc, char **argv) {
	struct cmd_results *result =  handle_command(argc, argv,
			"client.focused_tab_title",
			&config->border_colors.focused_tab_title, "#2e9ef4ff");
	if (result && result->status == CMD_SUCCESS) {
		config->has_focused_tab_title = true;
	}
	return result;
}
