#include <string.h>
#include <json-c/json.h>

#include "ipc-client.h"
#include "list.h"
#include "log.h"
#include "config.h"
#include "ipc.h"

static void ipc_parse_config(struct swaybar_config *config, const char *payload) {
	json_object *bar_config = json_tokener_parse(payload);
	json_object *tray_output, *mode, *hidden_state, *position, *status_command;
	json_object *font, *bar_height, *workspace_buttons, *strip_workspace_numbers;
	json_object *binding_mode_indicator, *verbose, *colors, *sep_symbol;
	json_object_object_get_ex(bar_config, "tray_output", &tray_output);
	json_object_object_get_ex(bar_config, "mode", &mode);
	json_object_object_get_ex(bar_config, "hidden_state", &hidden_state);
	json_object_object_get_ex(bar_config, "position", &position);
	json_object_object_get_ex(bar_config, "status_command", &status_command);
	json_object_object_get_ex(bar_config, "font", &font);
	json_object_object_get_ex(bar_config, "bar_height", &bar_height);
	json_object_object_get_ex(bar_config, "workspace_buttons", &workspace_buttons);
	json_object_object_get_ex(bar_config, "strip_workspace_numbers", &strip_workspace_numbers);
	json_object_object_get_ex(bar_config, "binding_mode_indicator", &binding_mode_indicator);
	json_object_object_get_ex(bar_config, "verbose", &verbose);
	json_object_object_get_ex(bar_config, "separator_symbol", &sep_symbol);
	json_object_object_get_ex(bar_config, "colors", &colors);

	if (status_command) {
		free(config->status_command);
		config->status_command = strdup(json_object_get_string(status_command));
	}

	if (position) {
		config->position = parse_position(json_object_get_string(position));
	}

	if (font) {
		free(config->font);
		config->font = parse_font(json_object_get_string(font));
	}

	if (sep_symbol) {
		free(config->sep_symbol);
		config->sep_symbol = strdup(json_object_get_string(sep_symbol));
	}

	if (strip_workspace_numbers) {
		config->strip_workspace_numbers = json_object_get_boolean(strip_workspace_numbers);
	}

	if (binding_mode_indicator) {
		config->binding_mode_indicator = json_object_get_boolean(binding_mode_indicator);
	}

	if (workspace_buttons) {
		config->workspace_buttons = json_object_get_boolean(workspace_buttons);
	}

	if (bar_height) {
		config->height = json_object_get_int(bar_height);
	}

	if (colors) {
		json_object *background, *statusline, *separator;
		json_object *focused_workspace_border, *focused_workspace_bg, *focused_workspace_text;
		json_object *inactive_workspace_border, *inactive_workspace_bg, *inactive_workspace_text;
		json_object *active_workspace_border, *active_workspace_bg, *active_workspace_text;
		json_object *urgent_workspace_border, *urgent_workspace_bg, *urgent_workspace_text;
		json_object *binding_mode_border, *binding_mode_bg, *binding_mode_text;
		json_object_object_get_ex(colors, "background", &background);
		json_object_object_get_ex(colors, "statusline", &statusline);
		json_object_object_get_ex(colors, "separator", &separator);
		json_object_object_get_ex(colors, "focused_workspace_border", &focused_workspace_border);
		json_object_object_get_ex(colors, "focused_workspace_bg", &focused_workspace_bg);
		json_object_object_get_ex(colors, "focused_workspace_text", &focused_workspace_text);
		json_object_object_get_ex(colors, "active_workspace_border", &active_workspace_border);
		json_object_object_get_ex(colors, "active_workspace_bg", &active_workspace_bg);
		json_object_object_get_ex(colors, "active_workspace_text", &active_workspace_text);
		json_object_object_get_ex(colors, "inactive_workspace_border", &inactive_workspace_border);
		json_object_object_get_ex(colors, "inactive_workspace_bg", &inactive_workspace_bg);
		json_object_object_get_ex(colors, "inactive_workspace_text", &inactive_workspace_text);
		json_object_object_get_ex(colors, "urgent_workspace_border", &urgent_workspace_border);
		json_object_object_get_ex(colors, "urgent_workspace_bg", &urgent_workspace_bg);
		json_object_object_get_ex(colors, "urgent_workspace_text", &urgent_workspace_text);
		json_object_object_get_ex(colors, "binding_mode_border", &binding_mode_border);
		json_object_object_get_ex(colors, "binding_mode_bg", &binding_mode_bg);
		json_object_object_get_ex(colors, "binding_mode_text", &binding_mode_text);
		if (background) {
			config->colors.background = parse_color(json_object_get_string(background));
		}

		if (statusline) {
			config->colors.statusline = parse_color(json_object_get_string(statusline));
		}

		if (separator) {
			config->colors.separator = parse_color(json_object_get_string(separator));
		}

		if (focused_workspace_border) {
			config->colors.focused_workspace.border = parse_color(json_object_get_string(focused_workspace_border));
		}

		if (focused_workspace_bg) {
			config->colors.focused_workspace.background = parse_color(json_object_get_string(focused_workspace_bg));
		}

		if (focused_workspace_text) {
			config->colors.focused_workspace.text = parse_color(json_object_get_string(focused_workspace_text));
		}

		if (active_workspace_border) {
			config->colors.active_workspace.border = parse_color(json_object_get_string(active_workspace_border));
		}

		if (active_workspace_bg) {
			config->colors.active_workspace.background = parse_color(json_object_get_string(active_workspace_bg));
		}

		if (active_workspace_text) {
			config->colors.active_workspace.text = parse_color(json_object_get_string(active_workspace_text));
		}

		if (inactive_workspace_border) {
			config->colors.inactive_workspace.border = parse_color(json_object_get_string(inactive_workspace_border));
		}

		if (inactive_workspace_bg) {
			config->colors.inactive_workspace.background = parse_color(json_object_get_string(inactive_workspace_bg));
		}

		if (inactive_workspace_text) {
			config->colors.inactive_workspace.text = parse_color(json_object_get_string(inactive_workspace_text));
		}

		if (binding_mode_border) {
			config->colors.binding_mode.border = parse_color(json_object_get_string(binding_mode_border));
		}

		if (binding_mode_bg) {
			config->colors.binding_mode.background = parse_color(json_object_get_string(binding_mode_bg));
		}

		if (binding_mode_text) {
			config->colors.binding_mode.text = parse_color(json_object_get_string(binding_mode_text));
		}
	}

	json_object_put(bar_config);
}

static void ipc_update_workspaces(struct swaybar_state *state) {
	if (state->output->workspaces) {
		list_foreach(state->output->workspaces, free_workspace);
		list_free(state->output->workspaces);
	}
	state->output->workspaces = create_list();

	uint32_t len = 0;
	char *res = ipc_single_command(state->ipc_socketfd, IPC_GET_WORKSPACES, NULL, &len);
	json_object *results = json_tokener_parse(res);
	if (!results) {
		free(res);
		return;
	}

	int i;
	int length = json_object_array_length(results);
	json_object *ws_json;
	json_object *num, *name, *visible, *focused, *out, *urgent;
	for (i = 0; i < length; ++i) {
		ws_json = json_object_array_get_idx(results, i);

		json_object_object_get_ex(ws_json, "num", &num);
		json_object_object_get_ex(ws_json, "name", &name);
		json_object_object_get_ex(ws_json, "visible", &visible);
		json_object_object_get_ex(ws_json, "focused", &focused);
		json_object_object_get_ex(ws_json, "output", &out);
		json_object_object_get_ex(ws_json, "urgent", &urgent);

		if (strcmp(json_object_get_string(out), state->output->name) == 0) {
			struct workspace *ws = malloc(sizeof(struct workspace));
			ws->num = json_object_get_int(num);
			ws->name = strdup(json_object_get_string(name));
			ws->visible = json_object_get_boolean(visible);
			ws->focused = json_object_get_boolean(focused);
			ws->urgent = json_object_get_boolean(urgent);
			list_add(state->output->workspaces, ws);
		}
	}

	json_object_put(results);
	free(res);
}

void ipc_bar_init(struct swaybar_state *state, int outputi, const char *bar_id) {
	uint32_t len = 0;
	char *res = ipc_single_command(state->ipc_socketfd, IPC_GET_OUTPUTS, NULL, &len);
	json_object *outputs = json_tokener_parse(res);
	json_object *info = json_object_array_get_idx(outputs, outputi);
	json_object *name;
	json_object_object_get_ex(info, "name", &name);
	state->output->name = strdup(json_object_get_string(name));
	free(res);
	json_object_put(outputs);

	len = strlen(bar_id);
	res = ipc_single_command(state->ipc_socketfd, IPC_GET_BAR_CONFIG, bar_id, &len);

	ipc_parse_config(state->config, res);
	free(res);

	const char *subscribe_json = "[ \"workspace\", \"mode\" ]";
	len = strlen(subscribe_json);
	res = ipc_single_command(state->ipc_event_socketfd, IPC_SUBSCRIBE, subscribe_json, &len);
	free(res);

	ipc_update_workspaces(state);
}

bool handle_ipc_event(struct swaybar_state *state) {
	struct ipc_response *resp = ipc_recv_response(state->ipc_event_socketfd);
	switch (resp->type) {
	case IPC_EVENT_WORKSPACE:
		ipc_update_workspaces(state);
		break;
	case IPC_EVENT_MODE: {
		json_object *result = json_tokener_parse(resp->payload);
		if (!result) {
			free_ipc_response(resp);
			sway_log(L_ERROR, "failed to parse payload as json");
			return false;
		}
		json_object *json_change;
		if (json_object_object_get_ex(result, "change", &json_change)) {
			const char *change = json_object_get_string(json_change);

			free(state->config->mode);
			if (strcmp(change, "default") == 0) {
				state->config->mode = NULL;
			} else {
				state->config->mode = strdup(change);
			}
		} else {
			sway_log(L_ERROR, "failed to parse response");
		}

		json_object_put(result);
		break;
	}
	default:
		free_ipc_response(resp);
		return false;
	}

	free_ipc_response(resp);
	return true;
}
