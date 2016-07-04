#ifndef _SWAY_IPC_JSON_H
#define _SWAY_IPC_JSON_H

#include <json-c/json.h>
#include "config.h"
#include "container.h"

json_object *ipc_json_get_version();
json_object *ipc_json_describe_bar_config(struct bar_config *bar);
json_object *ipc_json_describe_container(swayc_t *c);
json_object *ipc_json_describe_container_recursive(swayc_t *c);

#endif
