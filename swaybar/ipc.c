#include <string.h>
#include <json-c/json.h>
#include "swaybar/config.h"
#include "swaybar/ipc.h"
#include "ipc-client.h"
#include "list.h"
#include "log.h"

void ipc_send_workspace_command(const char *workspace_name) {
	uint32_t size = strlen("workspace \"\"") + strlen(workspace_name) + 1;

	char command[size];
	sprintf(command, "workspace \"%s\"", workspace_name);

	ipc_single_command(swaybar.ipc_socketfd, IPC_COMMAND, command, &size);
}

static void ipc_parse_config(struct config *config, const char *payload) {
	json_object *bar_config = json_tokener_parse(payload);
	json_object *tray_output, *mode, *hidden_bar, *position, *status_command;
	json_object *font, *bar_height, *wrap_scroll, *workspace_buttons, *strip_workspace_numbers;
	json_object *binding_mode_indicator, *verbose, *colors, *sep_symbol, *outputs;
	json_object *markup;
	json_object_object_get_ex(bar_config, "tray_output", &tray_output);
	json_object_object_get_ex(bar_config, "mode", &mode);
	json_object_object_get_ex(bar_config, "hidden_bar", &hidden_bar);
	json_object_object_get_ex(bar_config, "position", &position);
	json_object_object_get_ex(bar_config, "status_command", &status_command);
	json_object_object_get_ex(bar_config, "font", &font);
	json_object_object_get_ex(bar_config, "bar_height", &bar_height);
	json_object_object_get_ex(bar_config, "wrap_scroll", &wrap_scroll);
	json_object_object_get_ex(bar_config, "workspace_buttons", &workspace_buttons);
	json_object_object_get_ex(bar_config, "strip_workspace_numbers", &strip_workspace_numbers);
	json_object_object_get_ex(bar_config, "binding_mode_indicator", &binding_mode_indicator);
	json_object_object_get_ex(bar_config, "verbose", &verbose);
	json_object_object_get_ex(bar_config, "separator_symbol", &sep_symbol);
	json_object_object_get_ex(bar_config, "colors", &colors);
	json_object_object_get_ex(bar_config, "outputs", &outputs);
	json_object_object_get_ex(bar_config, "pango_markup", &markup);

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

	if (wrap_scroll) {
		config->wrap_scroll = json_object_get_boolean(wrap_scroll);
	}

	if (workspace_buttons) {
		config->workspace_buttons = json_object_get_boolean(workspace_buttons);
	}

	if (bar_height) {
		config->height = json_object_get_int(bar_height);
	}

	if (markup) {
		config->pango_markup = json_object_get_boolean(markup);
	}

	// free previous outputs list
	int i;
	for (i = 0; i < config->outputs->length; ++i) {
		free(config->outputs->items[i]);
	}
	list_free(config->outputs);
	config->outputs = create_list();

	if (outputs) {
		int length = json_object_array_length(outputs);
		json_object *output;
		const char *output_str;
		for (i = 0; i < length; ++i) {
			output = json_object_array_get_idx(outputs, i);
			output_str = json_object_get_string(output);
			if (strcmp("*", output_str) == 0) {
				config->all_outputs = true;
				break;
			}
			list_add(config->outputs, strdup(output_str));
		}
	} else {
		config->all_outputs = true;
	}

	if (colors) {
		json_object *background, *statusline, *separator;
		json_object *focused_background, *focused_statusline, *focused_separator;
		json_object *focused_workspace_border, *focused_workspace_bg, *focused_workspace_text;
		json_object *inactive_workspace_border, *inactive_workspace_bg, *inactive_workspace_text;
		json_object *active_workspace_border, *active_workspace_bg, *active_workspace_text;
		json_object *urgent_workspace_border, *urgent_workspace_bg, *urgent_workspace_text;
		json_object *binding_mode_border, *binding_mode_bg, *binding_mode_text;
		json_object_object_get_ex(colors, "background", &background);
		json_object_object_get_ex(colors, "statusline", &statusline);
		json_object_object_get_ex(colors, "separator", &separator);
		json_object_object_get_ex(colors, "focused_background", &focused_background);
		json_object_object_get_ex(colors, "focused_statusline", &focused_statusline);
		json_object_object_get_ex(colors, "focused_separator", &focused_separator);
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

		if (focused_background) {
			config->colors.focused_background = parse_color(json_object_get_string(focused_background));
		}

		if (focused_statusline) {
			config->colors.focused_statusline = parse_color(json_object_get_string(focused_statusline));
		}

		if (focused_separator) {
			config->colors.focused_separator = parse_color(json_object_get_string(focused_separator));
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

static void ipc_update_workspaces(struct bar *bar) {
	int i;
	for (i = 0; i < bar->outputs->length; ++i) {
		struct output *output = bar->outputs->items[i];
		if (output->workspaces) {
			free_workspaces(output->workspaces);
		}
		output->workspaces = create_list();
	}

	uint32_t len = 0;
	char *res = ipc_single_command(bar->ipc_socketfd, IPC_GET_WORKSPACES, NULL, &len);
	json_object *results = json_tokener_parse(res);
	if (!results) {
		free(res);
		return;
	}

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

		int j;
		for (j = 0; j < bar->outputs->length; ++j) {
			struct output *output = bar->outputs->items[j];
			if (strcmp(json_object_get_string(out), output->name) == 0) {
				struct workspace *ws = malloc(sizeof(struct workspace));
				ws->num = json_object_get_int(num);
				ws->name = strdup(json_object_get_string(name));
				ws->visible = json_object_get_boolean(visible);
				ws->focused = json_object_get_boolean(focused);
				if (ws->focused) {
					if (bar->focused_output) {
						bar->focused_output->focused = false;
					}
					bar->focused_output = output;
					output->focused = true;
				}
				ws->urgent = json_object_get_boolean(urgent);
				list_add(output->workspaces, ws);
			}
		}
	}

	json_object_put(results);
	free(res);
}

void ipc_bar_init(struct bar *bar, const char *bar_id) {
	// Get bar config
	uint32_t len = strlen(bar_id);
	char *res = ipc_single_command(bar->ipc_socketfd, IPC_GET_BAR_CONFIG, bar_id, &len);

	ipc_parse_config(bar->config, res);
	free(res);

	// Get outputs
	len = 0;
	res = ipc_single_command(bar->ipc_socketfd, IPC_GET_OUTPUTS, NULL, &len);
	json_object *outputs = json_tokener_parse(res);
	int i;
	int length = json_object_array_length(outputs);
	json_object *output, *output_name, *output_active;
	const char *name;
	bool active;
	for (i = 0; i < length; ++i) {
		output = json_object_array_get_idx(outputs, i);
		json_object_object_get_ex(output, "name", &output_name);
		json_object_object_get_ex(output, "active", &output_active);
		name = json_object_get_string(output_name);
		active = json_object_get_boolean(output_active);
		if (!active) {
			continue;
		}

		bool use_output = false;
		if (bar->config->all_outputs) {
			use_output = true;
		} else {
			int j = 0;
			for (j = 0; j < bar->config->outputs->length; ++j) {
				const char *conf_name = bar->config->outputs->items[j];
				if (strcasecmp(name, conf_name) == 0) {
					use_output = true;
					break;
				}
			}
		}

		if (!use_output) {
			continue;
		}

		// add bar to the output
		struct output *bar_output = new_output(name);
		bar_output->idx = i;
		list_add(bar->outputs, bar_output);
	}
	free(res);
	json_object_put(outputs);

	const char *subscribe_json = "[ \"workspace\", \"mode\" ]";
	len = strlen(subscribe_json);
	res = ipc_single_command(bar->ipc_event_socketfd, IPC_SUBSCRIBE, subscribe_json, &len);
	free(res);

	ipc_update_workspaces(bar);
}

bool handle_ipc_event(struct bar *bar) {
	struct ipc_response *resp = ipc_recv_response(bar->ipc_event_socketfd);
	switch (resp->type) {
	case IPC_EVENT_WORKSPACE:
		ipc_update_workspaces(bar);
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

			free(bar->config->mode);
			if (strcmp(change, "default") == 0) {
				bar->config->mode = NULL;
			} else {
				bar->config->mode = strdup(change);
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
