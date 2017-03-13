#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <libinput.h>
#include "sway/config.h"
#include "sway/input.h"
#include "list.h"
#include "log.h"

struct input_config *new_input_config(const char* identifier) {
	struct input_config *input = calloc(1, sizeof(struct input_config));
	if (!input) {
		sway_log(L_DEBUG, "Unable to allocate input config");
		return NULL;
	}
	sway_log(L_DEBUG, "new_input_config(%s)", identifier);
	if (!(input->identifier = strdup(identifier))) {
		free(input);
		sway_log(L_DEBUG, "Unable to allocate input config");
		return NULL;
	}

	input->tap = INT_MIN;
	input->drag_lock = INT_MIN;
	input->dwt = INT_MIN;
	input->send_events = INT_MIN;
	input->click_method = INT_MIN;
	input->middle_emulation = INT_MIN;
	input->natural_scroll = INT_MIN;
	input->accel_profile = INT_MIN;
	input->pointer_accel = FLT_MIN;
	input->scroll_method = INT_MIN;
	input->left_handed = INT_MIN;

	return input;
}

char *libinput_dev_unique_id(struct libinput_device *device) {
	int vendor = libinput_device_get_id_vendor(device);
	int product = libinput_device_get_id_product(device);
	char *name = strdup(libinput_device_get_name(device));

	char *p = name;
	for (; *p; ++p) {
		if (*p == ' ') {
			*p = '_';
		}
	}

	sway_log(L_DEBUG, "rewritten name %s", name);

	int len = strlen(name) + sizeof(char) * 6;
	char *identifier = malloc(len);
	if (!identifier) {
		sway_log(L_ERROR, "Unable to allocate unique input device name");
		return NULL;
	}

	const char *fmt = "%d:%d:%s";
	snprintf(identifier, len, fmt, vendor, product, name); 
	free(name);
	return identifier;
}

list_t *input_devices = NULL;
struct input_config *current_input_config = NULL;
