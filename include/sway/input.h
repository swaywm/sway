#ifndef _SWAY_INPUT_H
#define _SWAY_INPUT_H

#include <libinput.h>
#include "config.h"
#include "list.h"

struct input_config *new_input_config(const char* identifier);

char* libinput_dev_unique_id(struct libinput_device *dev);

/**
 * Global input device list.
 */
extern list_t *input_devices;

/**
 * Pointer used when reading input blocked.
 * Shared so that it can be cleared from commands.c when closing the block
 */
extern struct input_config *current_input_config;

#endif
