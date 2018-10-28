#include <json-c/json.h>
#include <stdbool.h>
#include "sway/commands.h"
#include "sway/desktop/transaction.h"
#include "sway/ipc-json.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "util.h"

json_object *ipc_sway_command(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {

	size_t payload_length = strlen(buf);
	char *line = strtok(buf, "\n");
	while (line) {
		size_t line_length = strlen(line);
		if (line + line_length >= buf + payload_length) {
			break;
		}
		line[line_length] = ';';
		line = strtok(NULL, "\n");
	}

	list_t *res_list = execute_command(buf, NULL, NULL);
	transaction_commit_dirty();
	json_object *reply = cmd_results_to_json(res_list);

	while (res_list->length) {
		struct cmd_results *results = res_list->items[0];
		free_cmd_results(results);
		list_del(res_list, 0);
	}
	list_free(res_list);
	return reply;
}

json_object *ipc_sway_send_tick(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	ipc_event_tick(buf);
	return ipc_json_success();
}

json_object *ipc_sway_get_outputs(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *outputs = json_object_new_array();
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		json_object *output_json = ipc_json_describe_node(&output->node);

		// override the default focused indicator because it's set
		// differently for the get_outputs reply
		struct sway_seat *seat = input_manager_get_default_seat();
		struct sway_workspace *focused_ws =
			seat_get_focused_workspace(seat);
		bool focused = focused_ws && output == focused_ws->output;
		json_object_object_del(output_json, "focused");
		json_object_object_add(output_json, "focused",
			json_object_new_boolean(focused));

		const char *subpixel = sway_wl_output_subpixel_to_string(output->wlr_output->subpixel);
		json_object_object_add(output_json, "subpixel_hinting", json_object_new_string(subpixel));
		json_object_array_add(outputs, output_json);
	}
	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (!output->enabled && output != root->noop_output) {
			json_object_array_add(outputs,
					ipc_json_describe_disabled_output(output));
		}
	}
	return outputs;
}

static void ipc_get_workspaces_callback(struct sway_workspace *workspace,
		void *data) {
	json_object *workspace_json = ipc_json_describe_node(&workspace->node);
	// override the default focused indicator because
	// it's set differently for the get_workspaces reply
	struct sway_seat *seat = input_manager_get_default_seat();
	struct sway_workspace *focused_ws = seat_get_focused_workspace(seat);
	bool focused = workspace == focused_ws;
	json_object_object_del(workspace_json, "focused");
	json_object_object_add(workspace_json, "focused",
			json_object_new_boolean(focused));
	json_object_array_add((json_object *)data, workspace_json);

	focused_ws = output_get_active_workspace(workspace->output);
	bool visible = workspace == focused_ws;
	json_object_object_add(workspace_json, "visible",
			json_object_new_boolean(visible));
}

json_object *ipc_sway_get_workspaces(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *workspaces = json_object_new_array();
	root_for_each_workspace(ipc_get_workspaces_callback, workspaces);
	return workspaces;
}

json_object *ipc_sway_subscribe(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	// TODO: Check if they're permitted to use these events
	struct json_object *request = json_tokener_parse(buf);
	if (request == NULL || !json_object_is_type(request, json_type_array)) {
		sway_log(SWAY_INFO, "Failed to parse subscribe request");
		return ipc_json_failure(NULL);
	}

	bool is_tick = false;
	// parse requested event types
	for (size_t i = 0; i < json_object_array_length(request); i++) {
		const char *event_type = json_object_get_string(
				json_object_array_get_idx(request, i));
		if (strcmp(event_type, "workspace") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_WORKSPACE);
		} else if (strcmp(event_type, "barconfig_update") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_BARCONFIG_UPDATE);
		} else if (strcmp(event_type, "bar_state_update") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_BAR_STATE_UPDATE);
		} else if (strcmp(event_type, "mode") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_MODE);
		} else if (strcmp(event_type, "shutdown") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_SHUTDOWN);
		} else if (strcmp(event_type, "window") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_WINDOW);
		} else if (strcmp(event_type, "binding") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_BINDING);
		} else if (strcmp(event_type, "tick") == 0) {
			client->subscribed_events |= event_mask(IPC_EVENT_TICK);
			is_tick = true;
		} else {
			json_object_put(request);
			sway_log(SWAY_INFO, "Unsupported event type in subscribe request");
			return ipc_json_failure(NULL);
		}
	}

	json_object_put(request);
	json_object *reply = ipc_json_success();
	if (is_tick) {
		const char *json_string = json_object_to_json_string(reply);
		ipc_send_reply(client, *type, json_string,
				(uint32_t)strlen(json_string));
		json_object_put(reply);

		reply = json_object_new_object();
		json_object_object_add(reply, "first", json_object_new_boolean(true));
		json_object_object_add(reply, "payload", json_object_new_string(""));
		*type = IPC_EVENT_TICK;
	}

	return reply;
}

json_object *ipc_sway_get_inputs(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *inputs = json_object_new_array();
	struct sway_input_device *device = NULL;
	wl_list_for_each(device, &server.input->devices, link) {
		json_object_array_add(inputs, ipc_json_describe_input(device));
	}
	return inputs;
}

json_object *ipc_sway_get_seats(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *seats = json_object_new_array();
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &server.input->seats, link) {
		json_object_array_add(seats, ipc_json_describe_seat(seat));
	}
	return seats;
}

json_object *ipc_sway_get_tree(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	return ipc_json_describe_node_recursive(&root->node);
}

static void ipc_get_marks_callback(struct sway_container *con, void *data) {
	json_object *marks = (json_object *)data;
	for (int i = 0; i < con->marks->length; ++i) {
		char *mark = (char *)con->marks->items[i];
		json_object_array_add(marks, json_object_new_string(mark));
	}
}

json_object *ipc_sway_get_marks(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *marks = json_object_new_array();
	root_for_each_container(ipc_get_marks_callback, marks);
	return marks;
}

json_object *ipc_sway_get_version(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	return ipc_json_get_version();
}

json_object *ipc_sway_get_bar_config(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	if (!buf[0]) {
		// Send list of configured bar IDs
		json_object *bars = json_object_new_array();
		for (int i = 0; i < config->bars->length; ++i) {
			struct bar_config *bar = config->bars->items[i];
			json_object_array_add(bars, json_object_new_string(bar->id));
		}
		return bars;
	} else {
		// Send particular bar's details
		struct bar_config *bar = NULL;
		for (int i = 0; i < config->bars->length; ++i) {
			bar = config->bars->items[i];
			if (strcmp(buf, bar->id) == 0) {
				break;
			}
			bar = NULL;
		}
		if (!bar) {
			return ipc_json_failure("No bar with that ID");
		}
		return ipc_json_describe_bar_config(bar);
	}
}

json_object *ipc_sway_get_binding_modes(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *modes = json_object_new_array();
	for (int i = 0; i < config->modes->length; i++) {
		struct sway_mode *mode = config->modes->items[i];
		json_object_array_add(modes, json_object_new_string(mode->name));
	}
	return modes;
}

json_object *ipc_sway_get_config(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	json_object *json = json_object_new_object();
	json_object_object_add(json, "config", json_object_new_string(config->current_config));
	return json;
}

json_object *ipc_sway_sync(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	// It was decided sway will not support this, just return success:false
	return ipc_json_failure(NULL);
}

static const ipc_handler sway_commands[] = {
	[IPC_COMMAND]           = ipc_sway_command,
	[IPC_GET_BAR_CONFIG]    = ipc_sway_get_bar_config,
	[IPC_GET_BINDING_MODES] = ipc_sway_get_binding_modes,
	[IPC_GET_CONFIG]        = ipc_sway_get_config,
	[IPC_GET_INPUTS]        = ipc_sway_get_inputs,
	[IPC_GET_MARKS]         = ipc_sway_get_marks,
	[IPC_GET_OUTPUTS]       = ipc_sway_get_outputs,
	[IPC_GET_SEATS]         = ipc_sway_get_seats,
	[IPC_GET_TREE]          = ipc_sway_get_tree,
	[IPC_GET_VERSION]       = ipc_sway_get_version,
	[IPC_GET_WORKSPACES]    = ipc_sway_get_workspaces,
	[IPC_SEND_TICK]         = ipc_sway_send_tick,
	[IPC_SUBSCRIBE]         = ipc_sway_subscribe,
	[IPC_SYNC]              = ipc_sway_sync,
};

const struct ipc_client_impl ipc_client_sway = {
	.num_commands = sizeof (sway_commands) / sizeof (*sway_commands),
	.commands = sway_commands,
};
