#ifndef _SWAY_IPC_SWAY_H
#define _SWAY_IPC_SWAY_H

#include <json-c/json.h>
#include "sway/ipc-server.h"

json_object *ipc_sway_command(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_bar_config(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_binding_modes(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_config(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_inputs(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_marks(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_outputs(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_seats(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_tree(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_version(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_get_workspaces(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_send_tick(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);
json_object *ipc_sway_subscribe(struct ipc_client *client,
		enum ipc_command_type *type, char *buf);

extern const ipc_json_descriptor_map ipc_json_sway_descriptors;

#endif
