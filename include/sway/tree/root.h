#ifndef _SWAY_ROOT_H
#define _SWAY_ROOT_H
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/wlr_texture.h>
#include "sway/tree/container.h"
#include "config.h"
#include "list.h"

extern struct sway_container root_container;

struct sway_root {
	struct wlr_output_layout *output_layout;

	struct wl_listener output_layout_change;
#ifdef HAVE_XWAYLAND
	struct wl_list xwayland_unmanaged; // sway_xwayland_unmanaged::link
#endif
	struct wl_list drag_icons; // sway_drag_icon::link

	struct wlr_texture *debug_tree;

	struct wl_list outputs; // sway_output::link

	list_t *scratchpad; // struct sway_container

	struct {
		struct wl_signal new_container;
	} events;
};

void root_create(void);

void root_destroy(void);

/**
 * Move a container to the scratchpad.
 */
void root_scratchpad_add_container(struct sway_container *con);

/**
 * Remove a container from the scratchpad.
 */
void root_scratchpad_remove_container(struct sway_container *con);

/**
 * Show a single scratchpad container.
 */
void root_scratchpad_show(struct sway_container *con);

/**
 * Hide a single scratchpad container.
 */
void root_scratchpad_hide(struct sway_container *con);

struct sway_container *root_workspace_for_pid(pid_t pid);

void root_record_workspace_pid(pid_t pid);

#endif
