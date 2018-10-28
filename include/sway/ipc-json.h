#ifndef _SWAY_IPC_JSON_H
#define _SWAY_IPC_JSON_H
#include <json.h>
#include "sway/tree/container.h"
#include "sway/input/input-manager.h"

typedef void (*ipc_json_descriptor)(struct sway_node *, json_object *);
typedef ipc_json_descriptor ipc_json_descriptor_map[NUM_NODE_TYPES];

json_object *ipc_json_success(void);
json_object *ipc_json_failure(const char *error);

json_object *ipc_json_get_version(void);

json_object *ipc_json_describe_disabled_output(struct sway_output *o);
json_object *ipc_json_describe_node(struct sway_node *node,
		const ipc_json_descriptor_map *desc);
json_object *ipc_json_describe_node_recursive(struct sway_node *node,
		const ipc_json_descriptor_map *desc);
json_object *ipc_json_describe_input(struct sway_input_device *device);
json_object *ipc_json_describe_seat(struct sway_seat *seat);
json_object *ipc_json_describe_bar_config(struct bar_config *bar);
void ipc_json_describe_container(struct sway_container *c, json_object *object);
void ipc_json_describe_view_common(struct sway_container *c, json_object *object);

void ipc_json_describe_output(struct sway_node *node, json_object *object);
void ipc_json_describe_root(struct sway_node *node, json_object *object);
void ipc_json_describe_workspace(struct sway_node *node, json_object *object);

#endif
