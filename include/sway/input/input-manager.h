#ifndef _SWAY_INPUT_INPUT_MANAGER_H
#define _SWAY_INPUT_INPUT_MANAGER_H
#include <libinput.h>
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
	struct input_config *config;
	struct wl_list link;
};

struct sway_input_manager {
	struct wl_listener input_add;
	struct wl_listener input_remove;
	struct sway_server *server;
	struct wl_list devices;
	struct wl_list seats;
};

struct sway_input_manager *sway_input_manager_create(
		struct sway_server *server);

bool sway_input_manager_has_focus(struct sway_input_manager *input,
		swayc_t *container);

void sway_input_manager_set_focus(struct sway_input_manager *input,
		swayc_t *container);

void sway_input_manager_configure_xcursor(struct sway_input_manager *input);

void sway_input_manager_apply_input_config(struct sway_input_manager *input,
		struct input_config *input_config);

void sway_input_manager_apply_seat_config(struct sway_input_manager *input,
		struct seat_config *seat_config);

struct sway_seat *sway_input_manager_get_default_seat(
		struct sway_input_manager *input);

#endif
