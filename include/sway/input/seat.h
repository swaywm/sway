#ifndef _SWAY_INPUT_SEAT_H
#define _SWAY_INPUT_SEAT_H

#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/edges.h>
#include "sway/input/input-manager.h"

struct sway_seat_device {
	struct sway_seat *sway_seat;
	struct sway_input_device *input_device;
	struct sway_keyboard *keyboard;
	struct wl_list link; // sway_seat::devices
};

struct sway_seat_node {
	struct sway_seat *seat;
	struct sway_node *node;

	struct wl_list link; // sway_seat::focus_stack

	struct wl_listener destroy;
};

struct sway_drag_icon {
	struct sway_seat *seat;
	struct wlr_drag_icon *wlr_drag_icon;
	struct wl_list link; // sway_root::drag_icons

	double x, y; // in layout-local coordinates

	struct wl_listener surface_commit;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
};

enum sway_seat_operation {
	OP_NONE,
	OP_DOWN,
	OP_MOVE_FLOATING,
	OP_MOVE_TILING,
	OP_RESIZE_FLOATING,
	OP_RESIZE_TILING,
};

struct sway_seat {
	struct wlr_seat *wlr_seat;
	struct sway_cursor *cursor;
	struct sway_input_manager *input;

	bool has_focus;
	struct wl_list focus_stack; // list of containers in focus order

	// If the focused layer is set, views cannot receive keyboard focus
	struct wlr_layer_surface_v1 *focused_layer;

	// If exclusive_client is set, no other clients will receive input events
	struct wl_client *exclusive_client;

	// Last touch point
	int32_t touch_id;
	double touch_x, touch_y;

	// Operations (drag and resize)
	enum sway_seat_operation operation;
	struct sway_container *op_container;
	struct sway_node *op_target_node; // target for tiling move
	enum wlr_edges op_target_edge;
	struct wlr_box op_drop_box;
	enum wlr_edges op_resize_edge;
	uint32_t op_button;
	bool op_resize_preserve_ratio;
	double op_ref_lx, op_ref_ly;         // cursor's x/y at start of op
	double op_ref_width, op_ref_height;  // container's size at start of op
	double op_ref_con_lx, op_ref_con_ly; // container's x/y at start of op
	bool op_moved;                       // if the mouse moved during a down op

	uint32_t last_button;
	uint32_t last_button_serial;

	struct wl_listener focus_destroy;
	struct wl_listener new_node;
	struct wl_listener new_drag_icon;

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

void seat_set_focus(struct sway_seat *seat, struct sway_node *node);

void seat_set_focus_container(struct sway_seat *seat,
		struct sway_container *con);

void seat_set_focus_workspace(struct sway_seat *seat,
		struct sway_workspace *ws);

void seat_set_focus_warp(struct sway_seat *seat,
		struct sway_node *node, bool warp, bool notify);

void seat_set_focus_surface(struct sway_seat *seat,
		struct wlr_surface *surface, bool unfocus);

void seat_set_focus_layer(struct sway_seat *seat,
		struct wlr_layer_surface_v1 *layer);

void seat_set_exclusive_client(struct sway_seat *seat,
		struct wl_client *client);

struct sway_node *seat_get_focus(struct sway_seat *seat);

struct sway_workspace *seat_get_focused_workspace(struct sway_seat *seat);

struct sway_container *seat_get_focused_container(struct sway_seat *seat);

/**
 * Return the last container to be focused for the seat (or the most recently
 * opened if no container has received focused) that is a child of the given
 * container. The focus-inactive container of the root window is the focused
 * container for the seat (if the seat does have focus). This function can be
 * used to determine what container gets focused next if the focused container
 * is destroyed, or focus moves to a container with children and we need to
 * descend into the next leaf in focus order.
 */
struct sway_node *seat_get_focus_inactive(struct sway_seat *seat,
		struct sway_node *node);

struct sway_container *seat_get_focus_inactive_tiling(struct sway_seat *seat,
		struct sway_workspace *workspace);

/**
 * Descend into the focus stack to find the focus-inactive view. Useful for
 * container placement when they change position in the tree.
 */
struct sway_container *seat_get_focus_inactive_view(struct sway_seat *seat,
		struct sway_node *ancestor);

/**
 * Return the immediate child of container which was most recently focused.
 */
struct sway_node *seat_get_active_tiling_child(struct sway_seat *seat,
		struct sway_node *parent);

/**
 * Iterate over the focus-inactive children of the container calling the
 * function on each.
 */
void seat_for_each_node(struct sway_seat *seat,
		void (*f)(struct sway_node *node, void *data), void *data);

void seat_apply_config(struct sway_seat *seat, struct seat_config *seat_config);

struct seat_config *seat_get_config(struct sway_seat *seat);

bool seat_is_input_allowed(struct sway_seat *seat, struct wlr_surface *surface);

void drag_icon_update_position(struct sway_drag_icon *icon);

void seat_begin_down(struct sway_seat *seat, struct sway_container *con,
		uint32_t button, double sx, double sy);

void seat_begin_move_floating(struct sway_seat *seat,
		struct sway_container *con, uint32_t button);

void seat_begin_move_tiling(struct sway_seat *seat,
		struct sway_container *con, uint32_t button);

void seat_begin_resize_floating(struct sway_seat *seat,
		struct sway_container *con, uint32_t button, enum wlr_edges edge);

void seat_begin_resize_tiling(struct sway_seat *seat,
		struct sway_container *con, uint32_t button, enum wlr_edges edge);

struct sway_container *seat_get_focus_inactive_floating(struct sway_seat *seat,
		struct sway_workspace *workspace);

void seat_end_mouse_operation(struct sway_seat *seat);

void seat_pointer_notify_button(struct sway_seat *seat, uint32_t time_msec,
		uint32_t button, enum wlr_button_state state);

#endif
