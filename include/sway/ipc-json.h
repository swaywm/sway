#ifndef _SWAY_IPC_JSON_H
#define _SWAY_IPC_JSON_H
#include <json-c/json.h>
#include "sway/tree/container.h"
#include "sway/input/input-manager.h"

json_object *ipc_json_get_version();

json_object *ipc_json_describe_container(struct sway_container *c);
json_object *ipc_json_describe_container_recursive(struct sway_container *c);
json_object *ipc_json_describe_input(struct sway_input_device *device);
json_object *ipc_json_describe_bar_config(struct bar_config *bar);

#endif
