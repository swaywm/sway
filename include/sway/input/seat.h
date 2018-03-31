#ifndef _SWAY_INPUT_SEAT_H
#define _SWAY_INPUT_SEAT_H

#include <wlr/types/wlr_seat.h>
#include "sway/input/input-manager.h"

struct sway_seat_device {
	struct sway_seat *sway_seat;
	struct sway_input_device *input_device;
	struct sway_keyboard *keyboard;
	struct seat_attachment_config *attachment_config;
	struct wl_list link; // sway_seat::devices
};

struct sway_seat_container {
	struct sway_seat *seat;
	struct sway_container *container;

	struct wl_list link; // sway_seat::focus_stack

	struct wl_listener destroy;
};

struct sway_seat {
	struct wlr_seat *wlr_seat;
	struct seat_config *config;
	struct sway_cursor *cursor;
	struct sway_input_manager *input;

	bool has_focus;
	struct wl_list focus_stack; // list of containers in focus order

	struct wl_listener focus_destroy;
	struct wl_listener new_container;

	struct wl_list devices; // sway_seat_device::link

	struct wl_list link; // input_manager::seats
};

struct sway_seat *sway_seat_create(struct sway_input_manager *input,
		const char *seat_name);

void sway_seat_destroy(struct sway_seat *seat);

void sway_seat_add_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_configure_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_configure_xcursor(struct sway_seat *seat);

void sway_seat_set_focus(struct sway_seat *seat, struct sway_container *container);

void sway_seat_set_focus_warp(struct sway_seat *seat,
		struct sway_container *container, bool warp);

struct sway_container *sway_seat_get_focus(struct sway_seat *seat);

/**
 * Return the last container to be focused for the seat (or the most recently
 * opened if no container has received focused) that is a child of the given
 * container. The focus-inactive container of the root window is the focused
 * container for the seat (if the seat does have focus). This function can be
 * used to determine what container gets focused next if the focused container
 * is destroyed, or focus moves to a container with children and we need to
 * descend into the next leaf in focus order.
 */
struct sway_container *sway_seat_get_focus_inactive(struct sway_seat *seat,
		struct sway_container *container);

struct sway_container *sway_seat_get_focus_by_type(struct sway_seat *seat,
		enum sway_container_type type);

void sway_seat_set_config(struct sway_seat *seat, struct seat_config *seat_config);

#endif
