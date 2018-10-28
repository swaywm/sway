#include <json-c/json.h>
#include "sway/config.h"
#include "sway/ipc-json.h"
#include "sway/ipc-sway.h"
#include "sway/tree/view.h"

static void ipc_i3_json_describe_view(struct sway_container *c, json_object *object) {
	const char *app_id = view_get_app_id(c->view);
	const char *class = app_id;
	const char *instance = app_id;
	const char *role = NULL;
	uint32_t parent_id = 0;
	uint32_t window_id = 0;

#if HAVE_XWAYLAND
	if (c->view->type == SWAY_VIEW_XWAYLAND) {
		class = view_get_class(c->view);
		instance = view_get_instance(c->view);
		role = view_get_window_role(c->view);
		parent_id = view_get_x11_parent_id(c->view);
		window_id = view_get_x11_window_id(c->view);
	}
#endif

	json_object_object_add(object, "window", json_object_new_int(window_id));

	json_object *window_props = json_object_new_object();

	if (class) {
		json_object_object_add(window_props, "class", json_object_new_string(class));
	}
	if (instance) {
		json_object_object_add(window_props, "instance", json_object_new_string(instance));
	}
	if (c->title) {
		json_object_object_add(window_props, "title", json_object_new_string(c->title));
	}
	if (role) {
		json_object_object_add(window_props, "window_role", json_object_new_string(role));
	}
	json_object_object_add(window_props, "transient_for",
			parent_id ? json_object_new_int(parent_id) : NULL);

	json_object_object_add(object, "window_properties", window_props);
}

static void ipc_i3_json_describe_container(struct sway_node *node, json_object *object) {
	struct sway_container *c = node->sway_container;
	ipc_json_describe_container(node->sway_container, object);
	if (c->view) {
		ipc_json_describe_view_common(node->sway_container, object);
		ipc_i3_json_describe_view(node->sway_container, object);
	}
}

const ipc_json_descriptor_map ipc_json_i3_descriptors = {
	[N_ROOT]        = ipc_json_describe_root,
	[N_WORKSPACE]   = ipc_json_describe_workspace,
	[N_OUTPUT]      = ipc_json_describe_output,
	[N_CONTAINER]   = ipc_i3_json_describe_container,
};

static json_object *ipc_i3_get_tree(struct ipc_client *client,
		enum ipc_command_type *type, char *buf) {
	return ipc_json_describe_node_recursive(&root->node, &ipc_json_i3_descriptors);
}

static const ipc_handler i3_commands[] = {
	[IPC_COMMAND]           = ipc_sway_command,
	[IPC_GET_BAR_CONFIG]    = ipc_sway_get_bar_config,
	[IPC_GET_BINDING_MODES] = ipc_sway_get_binding_modes,
	[IPC_GET_CONFIG]        = ipc_sway_get_config,
	[IPC_GET_MARKS]         = ipc_sway_get_marks,
	[IPC_GET_OUTPUTS]       = ipc_sway_get_outputs,
	[IPC_GET_TREE]          = ipc_i3_get_tree,
	[IPC_GET_VERSION]       = ipc_sway_get_version,
	[IPC_GET_WORKSPACES]    = ipc_sway_get_workspaces,
	[IPC_SEND_TICK]         = ipc_sway_send_tick,
	[IPC_SUBSCRIBE]         = ipc_sway_subscribe,
};

const struct ipc_client_impl ipc_client_i3 = {
	.num_commands = sizeof (i3_commands) / sizeof (*i3_commands),
	.commands = i3_commands,
};
