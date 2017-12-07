#ifndef _SWAY_INPUT_H
#define _SWAY_INPUT_H
#include <libinput.h>
#include "sway/server.h"
#include "config.h"
#include "list.h"

struct sway_input_manager {
	list_t *input_devices;
};

struct input_config *new_input_config(const char* identifier);

char* libinput_dev_unique_id(struct libinput_device *dev);

struct sway_input_manager *sway_input_manager_create(
		struct sway_server *server);

#endif
