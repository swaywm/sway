#ifndef _SWAY_INPUT_SEAT_H
#define _SWAY_INPUT_SEAT_H

#include <wlr/types/wlr_layer_shell.h>
#include <wlr/types/wlr_seat.h>
#include "sway/input/input-manager.h"

struct sway_seat_device {
	struct sway_seat *sway_seat;
	struct sway_input_device *input_device;
	struct sway_keyboard *keyboard;
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
	struct sway_cursor *cursor;
	struct sway_input_manager *input;

	bool has_focus;
	struct wl_list focus_stack; // list of containers in focus order

	// If the focused layer is set, views cannot receive keyboard focus
	struct wlr_layer_surface *focused_layer;

	// If exclusive_client is set, no other clients will receive input events
	struct wl_client *exclusive_client;

	struct wl_listener focus_destroy;
	struct wl_listener new_container;

	struct wl_list devices; // sway_seat_device::link

	struct wl_list link; // input_manager::seats
};

struct sway_seat *seat_create(struct sway_input_manager *input,
		const char *seat_name);

void seat_destroy(struct sway_seat *seat);

void seat_add_device(struct sway_seat *seat,
		struct sway_input_device *device);

void seat_configure_device(struct sway_seat *seat,
		struct sway_input_device *device);

void seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *device);

void seat_configure_xcursor(struct sway_seat *seat);

void seat_set_focus(struct sway_seat *seat, struct sway_container *container);

void seat_set_focus_warp(struct sway_seat *seat,
		struct sway_container *container, bool warp);

void seat_set_focus_layer(struct sway_seat *seat,
		struct wlr_layer_surface *layer);

void seat_set_exclusive_client(struct sway_seat *seat,
		struct wl_client *client);

struct sway_container *seat_get_focus(struct sway_seat *seat);

/**
 * Return the last container to be focused for the seat (or the most recently
 * opened if no container has received focused) that is a child of the given
 * container. The focus-inactive container of the root window is the focused
 * container for the seat (if the seat does have focus). This function can be
 * used to determine what container gets focused next if the focused container
 * is destroyed, or focus moves to a container with children and we need to
 * descend into the next leaf in focus order.
 */
struct sway_container *seat_get_focus_inactive(struct sway_seat *seat,
		struct sway_container *container);

struct sway_container *seat_get_focus_by_type(struct sway_seat *seat,
		struct sway_container *container, enum sway_container_type type);

void seat_apply_config(struct sway_seat *seat, struct seat_config *seat_config);

struct seat_config *seat_get_config(struct sway_seat *seat);

bool seat_allow_input(struct sway_seat *seat, struct wlr_surface *surface);

#endif
