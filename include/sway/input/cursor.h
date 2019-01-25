#ifndef _SWAY_INPUT_CURSOR_H
#define _SWAY_INPUT_CURSOR_H
#include <stdbool.h>
#include <stdint.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_surface.h>
#include "sway/input/seat.h"

#define SWAY_CURSOR_PRESSED_BUTTONS_CAP 32

#define SWAY_SCROLL_UP KEY_MAX + 1
#define SWAY_SCROLL_DOWN KEY_MAX + 2
#define SWAY_SCROLL_LEFT KEY_MAX + 3
#define SWAY_SCROLL_RIGHT KEY_MAX + 4

struct sway_cursor {
	struct sway_seat *seat;
	struct wlr_cursor *cursor;
	struct {
		double x, y;
		struct sway_node *node;
	} previous;
	struct wlr_xcursor_manager *xcursor_manager;

	const char *image;
	struct wl_client *image_client;
	struct wlr_surface *image_surface;
	int hotspot_x, hotspot_y;

	struct wlr_pointer_constraint_v1 *active_constraint;
	pixman_region32_t confine; // invalid if active_constraint == NULL

	struct wl_listener motion;
	struct wl_listener motion_absolute;
	struct wl_listener button;
	struct wl_listener axis;
	struct wl_listener frame;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;

	struct wl_listener tool_axis;
	struct wl_listener tool_tip;
	struct wl_listener tool_button;
	uint32_t tool_buttons;

	struct wl_listener request_set_cursor;

	struct wl_listener constraint_commit;

	struct wl_event_source *hide_source;
	bool hidden;

	// Mouse binding state
	uint32_t pressed_buttons[SWAY_CURSOR_PRESSED_BUTTONS_CAP];
	size_t pressed_button_count;
};

struct sway_node;

struct sway_node *node_at_coords(
		struct sway_seat *seat, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

void sway_cursor_destroy(struct sway_cursor *cursor);
struct sway_cursor *sway_cursor_create(struct sway_seat *seat);

/**
 * "Rebase" a cursor on top of whatever view is underneath it.
 *
 * This chooses a cursor icon and sends a motion event to the surface.
 */
void cursor_rebase(struct sway_cursor *cursor);

void cursor_handle_activity(struct sway_cursor *cursor);
void cursor_unhide(struct sway_cursor *cursor);
int cursor_get_timeout(struct sway_cursor *cursor);

/**
 * Like cursor_rebase, but also allows focus to change when the cursor enters a
 * new container.
 */
void cursor_send_pointer_motion(struct sway_cursor *cursor, uint32_t time_msec,
	struct sway_node *node, struct wlr_surface *surface, double sx, double sy);

void dispatch_cursor_button(struct sway_cursor *cursor,
	struct wlr_input_device *device, uint32_t time_msec, uint32_t button,
	enum wlr_button_state state);

void dispatch_cursor_axis(struct sway_cursor *cursor,
		struct wlr_event_pointer_axis *event);

void cursor_set_image(struct sway_cursor *cursor, const char *image,
	struct wl_client *client);

void cursor_set_image_surface(struct sway_cursor *cursor,
		struct wlr_surface *surface, int32_t hotspot_x, int32_t hotspot_y,
		struct wl_client *client);

void cursor_warp_to_container(struct sway_cursor *cursor,
	struct sway_container *container);

void cursor_warp_to_workspace(struct sway_cursor *cursor,
		struct sway_workspace *workspace);


void sway_cursor_constrain(struct sway_cursor *cursor,
	struct wlr_pointer_constraint_v1 *constraint);

uint32_t get_mouse_bindsym(const char *name, char **error);

uint32_t get_mouse_bindcode(const char *name, char **error);

// Considers both bindsym and bindcode
uint32_t get_mouse_button(const char *name, char **error);

const char *get_mouse_button_name(uint32_t button);

#endif
