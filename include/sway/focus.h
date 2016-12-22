#ifndef _SWAY_FOCUS_H
#define _SWAY_FOCUS_H
enum movement_direction {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_UP,
	MOVE_DOWN,
	MOVE_PARENT,
	MOVE_CHILD,
	MOVE_NEXT,
	MOVE_PREV
};

#include "container.h"

// focused_container - the container found by following the `focused` pointer
// from a given container to a container with `is_focused` boolean set
// ---
// focused_view - the container found by following the `focused` pointer from a
// given container to a view.
// ---

swayc_t *get_focused_container(swayc_t *parent);
swayc_t *get_focused_view(swayc_t *parent);
swayc_t *get_focused_float(swayc_t *ws);

// a special-case function to get the focused view, regardless
// of whether it's tiled or floating
swayc_t *get_focused_view_include_floating(swayc_t *parent);

bool set_focused_container(swayc_t *container);
bool set_focused_container_for(swayc_t *ancestor, swayc_t *container);

// lock focused container/view. locked by windows with OVERRIDE attribute
// and unlocked when they are destroyed

extern bool locked_container_focus;

// Prevents wss from being destroyed on focus switch
extern bool suspend_workspace_cleanup;

bool move_focus(enum movement_direction direction);

#endif
