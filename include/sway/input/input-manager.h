#ifndef _SWAY_INPUT_INPUT_MANAGER_H
#define _SWAY_INPUT_INPUT_MANAGER_H
#include <libinput.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include "sway/server.h"
#include "sway/config.h"
#include "list.h"

/**
 * The global singleton input manager
 * TODO: make me not a global
 */
extern struct sway_input_manager *input_manager;

struct sway_input_device {
	char *identifier;
	struct wlr_input_device *wlr_device;
	struct wl_list link;
	struct wl_listener device_destroy;
};

struct sway_input_manager {
	struct sway_server *server;
	struct wl_list devices;
	struct wl_list seats;

	struct wlr_input_inhibit_manager *inhibit;
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;

	struct wl_listener new_input;
	struct wl_listener inhibit_activate;
	struct wl_listener inhibit_deactivate;
	struct wl_listener virtual_keyboard_new;
};

struct sway_input_manager *input_manager_create(struct sway_server *server);

bool input_manager_has_focus(struct sway_input_manager *input,
		struct sway_container *container);

void input_manager_set_focus(struct sway_input_manager *input,
		struct sway_container *container);

void input_manager_configure_xcursor(struct sway_input_manager *input);

void input_manager_apply_input_config(struct sway_input_manager *input,
		struct input_config *input_config);

void input_manager_apply_seat_config(struct sway_input_manager *input,
		struct seat_config *seat_config);

struct sway_seat *input_manager_get_default_seat(
		struct sway_input_manager *input);

struct sway_seat *input_manager_get_seat(struct sway_input_manager *input,
		const char *seat_name);

/**
 * Gets the last seat the user interacted with
 */
struct sway_seat *input_manager_current_seat(struct sway_input_manager *input);

struct input_config *input_device_get_config(struct sway_input_device *device);


#endif
