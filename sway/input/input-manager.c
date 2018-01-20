#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <libinput.h>
#include <math.h>
#include <wlr/backend/libinput.h>
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

static const char *default_seat = "seat0";

// TODO make me not global
struct sway_input_manager *input_manager;

struct input_config *current_input_config = NULL;
struct seat_config *current_seat_config = NULL;

static struct sway_seat *input_manager_get_seat(
		struct sway_input_manager *input, const char *seat_name) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		if (strcmp(seat->wlr_seat->name, seat_name) == 0) {
			return seat;
		}
	}

	return sway_seat_create(input, seat_name);
}

static char *get_device_identifier(struct wlr_input_device *device) {
	int vendor = device->vendor;
	int product = device->product;
	char *name = strdup(device->name);
	name = strip_whitespace(name);

	char *p = name;
	for (; *p; ++p) {
		if (*p == ' ') {
			*p = '_';
		}
	}

	const char *fmt = "%d:%d:%s";
	int len = snprintf(NULL, 0, fmt, vendor, product, name) + 1;
	char *identifier = malloc(len);
	if (!identifier) {
		wlr_log(L_ERROR, "Unable to allocate unique input device name");
		return NULL;
	}

	snprintf(identifier, len, fmt, vendor, product, name);
	free(name);
	return identifier;
}

static struct sway_input_device *input_sway_device_from_wlr(
		struct sway_input_manager *input, struct wlr_input_device *device) {
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &input->devices, link) {
		if (input_device->wlr_device == device) {
			return input_device;
		}
	}
	return NULL;
}

static bool input_has_seat_configuration(struct sway_input_manager *input) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		if (seat->config) {
			return true;
		}
	}

	return false;
}

static void sway_input_manager_libinput_config_pointer(struct sway_input_device *input_device) {
	struct wlr_input_device *wlr_device = input_device->wlr_device;
	struct input_config *ic = input_device->config;
	struct libinput_device *libinput_device;

	if (!ic || !wlr_input_device_is_libinput(wlr_device)) {
		return;
	}

	libinput_device = wlr_libinput_get_device_handle(wlr_device);
	wlr_log(L_DEBUG, "sway_input_manager_libinput_config_pointer(%s)", ic->identifier);

	if (ic->accel_profile != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) accel_set_profile(%d)",
			ic->identifier, ic->accel_profile);
		libinput_device_config_accel_set_profile(libinput_device, ic->accel_profile);
	}
	if (ic->click_method != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) click_set_method(%d)",
			ic->identifier, ic->click_method);
		libinput_device_config_click_set_method(libinput_device, ic->click_method);
	}
	if (ic->drag_lock != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) tap_set_drag_lock_enabled(%d)",
			ic->identifier, ic->click_method);
		libinput_device_config_tap_set_drag_lock_enabled(libinput_device, ic->drag_lock);
	}
	if (ic->dwt != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) dwt_set_enabled(%d)",
			ic->identifier, ic->dwt);
		libinput_device_config_dwt_set_enabled(libinput_device, ic->dwt);
	}
	if (ic->left_handed != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) left_handed_set_enabled(%d)",
			ic->identifier, ic->left_handed);
		libinput_device_config_left_handed_set(libinput_device, ic->left_handed);
	}
	if (ic->middle_emulation != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) middle_emulation_set_enabled(%d)",
			ic->identifier, ic->middle_emulation);
		libinput_device_config_middle_emulation_set_enabled(libinput_device, ic->middle_emulation);
	}
	if (ic->natural_scroll != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) natural_scroll_set_enabled(%d)",
			ic->identifier, ic->natural_scroll);
		libinput_device_config_scroll_set_natural_scroll_enabled(libinput_device, ic->natural_scroll);
	}
	if (ic->pointer_accel != FLT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) accel_set_speed(%f)",
			ic->identifier, ic->pointer_accel);
		libinput_device_config_accel_set_speed(libinput_device, ic->pointer_accel);
	}
	if (ic->scroll_method != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) scroll_set_method(%d)",
			ic->identifier, ic->scroll_method);
		libinput_device_config_scroll_set_method(libinput_device, ic->scroll_method);
	}
	if (ic->send_events != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) send_events_set_mode(%d)",
			ic->identifier, ic->send_events);
		libinput_device_config_send_events_set_mode(libinput_device, ic->send_events);
	}
	if (ic->tap != INT_MIN) {
		wlr_log(L_DEBUG, "libinput_config_pointer(%s) tap_set_enabled(%d)",
			ic->identifier, ic->tap);
		libinput_device_config_tap_set_enabled(libinput_device, ic->tap);
	}
}

static void input_add_notify(struct wl_listener *listener, void *data) {
	struct sway_input_manager *input =
		wl_container_of(listener, input, input_add);
	struct wlr_input_device *device = data;

	struct sway_input_device *input_device =
		calloc(1, sizeof(struct sway_input_device));
	if (!sway_assert(input_device, "could not allocate input device")) {
		return;
	}

	input_device->wlr_device = device;
	input_device->identifier = get_device_identifier(device);
	wl_list_insert(&input->devices, &input_device->link);

	wlr_log(L_DEBUG, "adding device: '%s'",
		input_device->identifier);

	// find config
	for (int i = 0; i < config->input_configs->length; ++i) {
		struct input_config *input_config = config->input_configs->items[i];
		if (strcmp(input_config->identifier, input_device->identifier) == 0) {
			input_device->config = input_config;
			break;
		}
	}

	if (input_device->wlr_device->type == WLR_INPUT_DEVICE_POINTER) {
		sway_input_manager_libinput_config_pointer(input_device);
	}

	struct sway_seat *seat = NULL;
	if (!input_has_seat_configuration(input)) {
		wlr_log(L_DEBUG, "no seat configuration, using default seat");
		seat = input_manager_get_seat(input, default_seat);
		sway_seat_add_device(seat, input_device);
		return;
	}

	bool added = false;
	wl_list_for_each(seat, &input->seats, link) {
		bool has_attachment = seat->config &&
			(seat_config_get_attachment(seat->config, input_device->identifier) ||
			 seat_config_get_attachment(seat->config, "*"));

		if (has_attachment) {
			sway_seat_add_device(seat, input_device);
			added = true;
		}
	}

	if (!added) {
		wl_list_for_each(seat, &input->seats, link) {
			if (seat->config && seat->config->fallback == 1) {
				sway_seat_add_device(seat, input_device);
				added = true;
			}
		}
	}

	if (!added) {
		wlr_log(L_DEBUG,
			"device '%s' is not configured on any seats",
			input_device->identifier);
	}
}

static void input_remove_notify(struct wl_listener *listener, void *data) {
	struct sway_input_manager *input =
		wl_container_of(listener, input, input_remove);
	struct wlr_input_device *device = data;

	struct sway_input_device *input_device =
		input_sway_device_from_wlr(input, device);

	if (!sway_assert(input_device, "could not find sway device")) {
		return;
	}

	wlr_log(L_DEBUG, "removing device: '%s'",
		input_device->identifier);

	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		sway_seat_remove_device(seat, input_device);
	}

	wl_list_remove(&input_device->link);
	free(input_device->identifier);
	free(input_device);
}

struct sway_input_manager *sway_input_manager_create(
		struct sway_server *server) {
	struct sway_input_manager *input =
		calloc(1, sizeof(struct sway_input_manager));
	if (!input) {
		return NULL;
	}
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

void sway_input_manager_apply_input_config(struct sway_input_manager *input,
		struct input_config *input_config) {
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &input->devices, link) {
		if (strcmp(input_device->identifier, input_config->identifier) == 0) {
			input_device->config = input_config;

			if (input_device->wlr_device->type == WLR_INPUT_DEVICE_POINTER) {
				sway_input_manager_libinput_config_pointer(input_device);
			}

			struct sway_seat *seat = NULL;
			wl_list_for_each(seat, &input->seats, link) {
				sway_seat_configure_device(seat, input_device);
			}
		}
	}
}

void sway_input_manager_apply_seat_config(struct sway_input_manager *input,
		struct seat_config *seat_config) {
	wlr_log(L_DEBUG, "applying new seat config for seat %s",
		seat_config->name);
	struct sway_seat *seat = input_manager_get_seat(input, seat_config->name);
	if (!seat) {
		return;
	}

	sway_seat_set_config(seat, seat_config);

	// for every device, try to add it to a seat and if no seat has it
	// attached, add it to the fallback seats.
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &input->devices, link) {
		list_t *seat_list = create_list();
		struct sway_seat *seat = NULL;
		wl_list_for_each(seat, &input->seats, link) {
			if (!seat->config) {
				continue;
			}
			if (seat_config_get_attachment(seat->config, "*") ||
					seat_config_get_attachment(seat->config,
						input_device->identifier)) {
				list_add(seat_list, seat);
			}
		}

		if (seat_list->length) {
			wl_list_for_each(seat, &input->seats, link) {
				bool attached = false;
				for (int i = 0; i < seat_list->length; ++i) {
					if (seat == seat_list->items[i]) {
						attached = true;
						break;
					}
				}
				if (attached) {
					sway_seat_add_device(seat, input_device);
				} else {
					sway_seat_remove_device(seat, input_device);
				}
			}
		} else {
			wl_list_for_each(seat, &input->seats, link) {
				if (seat->config && seat->config->fallback == 1) {
					sway_seat_add_device(seat, input_device);
				} else {
					sway_seat_remove_device(seat, input_device);
				}
			}
		}
		list_free(seat_list);
	}
}

void sway_input_manager_configure_xcursor(struct sway_input_manager *input) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		sway_seat_configure_xcursor(seat);
	}
}

struct sway_seat *sway_input_manager_get_default_seat(
		struct sway_input_manager *input) {
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input->seats, link) {
		if (strcmp(seat->wlr_seat->name, "seat0") == 0) {
			return seat;
		}
	}
	return seat;
}
