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

// TODO make me not global
struct sway_input_manager *input_manager;

struct input_config *current_input_config = NULL;

static struct sway_seat *input_manager_get_seat(
		struct sway_input_manager *input, const char *seat_name) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		if (strcmp(seat->seat->name, seat_name) == 0) {
			return seat;
		}
	}

	return sway_seat_create(input, seat_name);
}

static char *get_device_identifier(struct wlr_input_device *device) {
	int vendor = device->vendor;
	int product = device->product;
	char *name = strdup(device->name);

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

static struct sway_input_device *input_sway_device_from_wlr(struct sway_input_manager *input,
		struct wlr_input_device *device) {
	struct sway_input_device *sway_device = NULL;
	wl_list_for_each(sway_device, &input->devices, link) {
		if (sway_device->wlr_device == device) {
			return sway_device;
		}
	}
	return NULL;
}

static struct sway_input_device *input_sway_device_from_config(struct sway_input_manager *input,
		struct input_config *config) {
	struct sway_input_device *sway_device = NULL;
	wl_list_for_each(sway_device, &input->devices, link) {
		if (strcmp(sway_device->identifier, config->identifier) == 0) {
			return sway_device;
		}
	}
	return NULL;
}

static void input_add_notify(struct wl_listener *listener, void *data) {
	struct sway_input_manager *input =
		wl_container_of(listener, input, input_add);
	struct wlr_input_device *device = data;

	struct sway_input_device *sway_device =
		calloc(1, sizeof(struct sway_input_device));
	if (!sway_assert(sway_device, "could not allocate input device")) {
		return;
	}

	sway_device->wlr_device = device;
	sway_device->identifier = get_device_identifier(device);
	wl_list_insert(&input->devices, &sway_device->link);

	// find config
	for (int i = 0; i < config->input_configs->length; ++i) {
		struct input_config *input_config = config->input_configs->items[i];
		if (strcmp(input_config->identifier, sway_device->identifier) == 0) {
			sway_device->config = input_config;
			break;
		}
	}

	const char *seat_name =
		(sway_device->config ? sway_device->config->seat : default_seat);
	struct sway_seat *seat = input_manager_get_seat(input, seat_name);
	sway_seat_add_device(seat, sway_device);
}

static void input_remove_notify(struct wl_listener *listener, void *data) {
	struct sway_input_manager *input =
		wl_container_of(listener, input, input_remove);
	struct wlr_input_device *device = data;

	struct sway_input_device *sway_device =
		input_sway_device_from_wlr(input, device);

	if (!sway_assert(sway_device, "could not find sway device")) {
		return;
	}

	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		sway_seat_remove_device(seat, sway_device);
	}

	wl_list_remove(&sway_device->link);
	free(sway_device->identifier);
	free(sway_device);
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

	wl_list_init(&input->devices);
	wl_list_init(&input->seats);

	// create the default seat
	input_manager_get_seat(input, default_seat);

	input->input_add.notify = input_add_notify;
	wl_signal_add(&server->backend->events.input_add, &input->input_add);

	input->input_remove.notify = input_remove_notify;
	wl_signal_add(&server->backend->events.input_remove, &input->input_remove);

	return input;
}

bool sway_input_manager_has_focus(struct sway_input_manager *input,
		swayc_t *container) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		if (seat->focus == container) {
			return true;
		}
	}

	return false;
}

void sway_input_manager_set_focus(struct sway_input_manager *input,
		swayc_t *container) {
	struct sway_seat *seat ;
	wl_list_for_each(seat, &input->seats, link) {
		sway_seat_set_focus(seat, container);
	}
}

void sway_input_manager_apply_config(struct sway_input_manager *input,
		struct input_config *input_config) {
	struct sway_input_device *sway_device =
		input_sway_device_from_config(input, input_config);
	if (!sway_device) {
		return;
	}

	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		sway_seat_remove_device(seat, sway_device);
	}

	const char *seat_name = (input_config->seat ? input_config->seat : default_seat);
	seat = input_manager_get_seat(input, seat_name);
	sway_seat_add_device(seat, sway_device);
}

void sway_input_manager_configure_xcursor(struct sway_input_manager *input) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		sway_seat_configure_xcursor(seat);
	}
}
