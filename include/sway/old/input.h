#ifndef _SWAY_INPUT_H
#define _SWAY_INPUT_H
#include <libinput.h>
#include "sway/server.h"
#include "config.h"
#include "list.h"

struct sway_input {
	list_t *input_devices;
};

struct input_config *new_input_config(const char* identifier);

char* libinput_dev_unique_id(struct libinput_device *dev);

struct sway_input *sway_input_create(struct sway_server *server);

/**
 * Pointer used when reading input blocked.
 * Shared so that it can be cleared from commands.c when closing the block
 */
extern struct input_config *current_input_config;

#endif
