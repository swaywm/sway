#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <libinput.h>
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "list.h"
#include "log.h"

static const char *default_seat = "seat0";

struct input_config *current_input_config = NULL;

static struct sway_seat *input_manager_get_seat(
		struct sway_input_manager *input, const char *seat_name) {
	struct sway_seat *seat = NULL;

	for (int i = 0; i < input->seats->length; ++i) {
		seat = input->seats->items[i];
		if (strcmp(seat->seat->name, seat_name) == 0) {
			return seat;
		}
	}

	seat = sway_seat_create(input, seat_name);
	list_add(input->seats, seat);

	return seat;
}

static void input_add_notify(struct wl_listener *listener, void *data) {
	struct sway_input_manager *input =
		wl_container_of(listener, input, input_add);
	struct wlr_input_device *device = data;

	// TODO device configuration
	struct sway_seat *seat = input_manager_get_seat(input, default_seat);
	sway_seat_add_device(seat, device);
}

static void input_remove_notify(struct wl_listener *listener, void *data) {
	struct sway_input_manager *input =
		wl_container_of(listener, input, input_remove);
	struct wlr_input_device *device = data;

	// TODO device configuration
	struct sway_seat *seat = input_manager_get_seat(input, default_seat);
	sway_seat_remove_device(seat, device);
}

struct sway_input_manager *sway_input_manager_create(
		struct sway_server *server) {
	struct sway_input_manager *input =
		calloc(1, sizeof(struct sway_input_manager));
	if (!input) {
		return NULL;
	}
	// XXX probably don't need the full server
	input->server = server;

	input->seats = create_list();

	// create the default seat
	input_manager_get_seat(input, default_seat);

	input->input_add.notify = input_add_notify;
	wl_signal_add(&server->backend->events.input_add, &input->input_add);

	input->input_remove.notify = input_remove_notify;
	wl_signal_add(&server->backend->events.input_remove, &input->input_remove);

	return input;
}

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

bool sway_input_manager_swayc_has_focus(struct sway_input_manager *input,
		swayc_t *container) {
	for (int i = 0; i < input->seats->length; ++i) {
		struct sway_seat *seat = input->seats->items[i];
		if (seat->focus == container) {
			return true;
		}
	}

	return false;
}
