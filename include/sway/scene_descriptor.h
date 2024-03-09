/**
 * Across a wayland compositor, there are multiple shells: It can be
 * a toplevel, or a layer_shell, or even something more meta like a drag
 * icon or highlight indicators when dragging windows around.
 *
 * This object lets us store values that represent these modes of operation
 * and keep track of what object is being represented.
 */
#ifndef _SWAY_SCENE_DESCRIPTOR_H
#define _SWAY_SCENE_DESCRIPTOR_H
#include <wlr/types/wlr_scene.h>

enum sway_scene_descriptor_type {
	SWAY_SCENE_DESC_BUFFER_TIMER,
	SWAY_SCENE_DESC_NON_INTERACTIVE,
	SWAY_SCENE_DESC_CONTAINER,
	SWAY_SCENE_DESC_VIEW,
	SWAY_SCENE_DESC_LAYER_SHELL,
	SWAY_SCENE_DESC_XWAYLAND_UNMANAGED,
	SWAY_SCENE_DESC_POPUP,
	SWAY_SCENE_DESC_DRAG_ICON,
};

bool scene_descriptor_assign(struct wlr_scene_node *node,
	enum sway_scene_descriptor_type type, void *data);

void *scene_descriptor_try_get(struct wlr_scene_node *node,
	enum sway_scene_descriptor_type type);

void scene_descriptor_destroy(struct wlr_scene_node *node,
	enum sway_scene_descriptor_type type);

#endif
