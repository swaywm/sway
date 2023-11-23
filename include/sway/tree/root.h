#ifndef _SWAY_ROOT_H
#define _SWAY_ROOT_H
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/render/wlr_texture.h>
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "config.h"
#include "list.h"

extern struct sway_root *root;

struct sway_root {
	struct sway_node node;
	struct wlr_output_layout *output_layout;

	struct wl_listener output_layout_change;

	// scene node layout:
	// - root
	// 	- staging
	// 	- layer shell stuff
	// 	- tiling
	// 	- floating
	// 	- fullscreen stuff
	// 	- seat stuff
	// 	- ext_session_lock
	struct wlr_scene *root_scene;

	// since wlr_scene nodes can't be orphaned and must always
	// have a parent, use this staging scene_tree so that a
	// node always have a valid parent. Nothing in this
	// staging node will be visible.
	struct wlr_scene_tree *staging;

	// tree containing all layers the compositor will render. Cursor handling
	// will end up iterating this tree.
	struct wlr_scene_tree *layer_tree;

	struct {
		struct wlr_scene_tree *tiling;
		struct wlr_scene_tree *floating;
		struct wlr_scene_tree *fullscreen;
		struct wlr_scene_tree *fullscreen_global;
		struct wlr_scene_tree *seat;
		struct wlr_scene_tree *session_lock;
	} layers;

#if HAVE_XWAYLAND
	struct wl_list xwayland_unmanaged; // sway_xwayland_unmanaged::link
#endif

	// Includes disabled outputs
	struct wl_list all_outputs; // sway_output::link

	double x, y;
	double width, height;

	list_t *outputs; // struct sway_output
	list_t *non_desktop_outputs; // struct sway_output_non_desktop
	list_t *scratchpad; // struct sway_container

	// For when there's no connected outputs
	struct sway_output *fallback_output;

	struct sway_container *fullscreen_global;

	struct {
		struct wl_signal new_node;
	} events;
};

struct sway_root *root_create(struct wl_display *display);

void root_destroy(struct sway_root *root);

/**
 * Move a container to the scratchpad.
 * If a workspace is passed, the container is assumed to have been in
 * the scratchpad before and is shown on the workspace.
 * The ws parameter can safely be NULL.
 */
void root_scratchpad_add_container(struct sway_container *con,
   struct sway_workspace *ws);

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

void root_for_each_workspace(void (*f)(struct sway_workspace *ws, void *data),
		void *data);

void root_for_each_container(void (*f)(struct sway_container *con, void *data),
		void *data);

struct sway_output *root_find_output(
		bool (*test)(struct sway_output *output, void *data), void *data);

struct sway_workspace *root_find_workspace(
		bool (*test)(struct sway_workspace *ws, void *data), void *data);

struct sway_container *root_find_container(
		bool (*test)(struct sway_container *con, void *data), void *data);

void root_get_box(struct sway_root *root, struct wlr_box *box);

#endif
