#include <wlc/wlc.h>

#include "focus.h"
#include "log.h"
#include "workspace.h"
#include "layout.h"

bool locked_container_focus = false;
bool locked_view_focus = false;

// switches parent focus to c. will switch it accordingly
// TODO: Everything needs a handle, so we can set front/back position properly
static void update_focus(swayc_t *c) {
	// Handle if focus switches
	swayc_t *parent = c->parent;
	if (parent->focused != c) {
		switch (c->type) {
		// Shouldnt happen
		case C_ROOT: return;

		// Case where output changes
		case C_OUTPUT:
			wlc_output_focus(c->handle);
			// Set new workspace to the outputs focused workspace
			active_workspace = c->focused;
			break;

		// Case where workspace changes
		case C_WORKSPACE:
			if (parent->focused) {
				swayc_t *ws = parent->focused;
				// hide visibility of old workspace
				uint32_t mask = 1;
				container_map(ws, set_view_visibility, &mask);
				// set visibility of new workspace
				mask = 2;
				container_map(c, set_view_visibility, &mask);
				wlc_output_set_mask(parent->handle, 2);
				c->parent->focused = c;
				destroy_workspace(ws);
			}
			active_workspace = c;
			break;

		default:
		case C_VIEW:
		case C_CONTAINER:
			// TODO whatever to do when container changes
			// for example, stacked and tabbing change stuff.
			break;
		}
	}
	c->parent->focused = c;
}

bool move_focus(enum movement_direction direction) {
	swayc_t *view = get_focused_container(&root_container);
	view = swayc_by_direction(view, direction);
	if (view) {
		if (direction == MOVE_PARENT) {
			set_focused_container(view);
		} else {
			set_focused_container(get_focused_view(view));
		}
		return true;
	}
	return false;
}

swayc_t *get_focused_container(swayc_t *parent) {
	while (parent && !parent->is_focused) {
		parent = parent->focused;
	}
	// just incase
	if (parent == NULL) {
		sway_log(L_DEBUG, "get_focused_container unable to find container");
		return active_workspace;
	}
	return parent;
}

void set_focused_container(swayc_t *c) {
	if (locked_container_focus || !c) {
		return;
	}
	sway_log(L_DEBUG, "Setting focus to %p:%ld", c, c->handle);
	// Get workspace for c, get that workspaces current focused container.
	// if that focsued container is fullscreen dont change focus
	swayc_t *workspace = swayc_parent_by_type(c, C_WORKSPACE);
	swayc_t *focused = get_focused_view(workspace);
	if (swayc_is_fullscreen(focused)) {
		return;
	}

	// update container focus from here to root, making necessary changes along
	// the way
	swayc_t *p = c;
	if (p->type != C_OUTPUT && p->type != C_ROOT) {
		p->is_focused = true;
	}
	while (p != &root_container) {
		update_focus(p);
		p = p->parent;
		p->is_focused = false;
	}

	// get new focused view and set focus to it.
	p = get_focused_view(c);
	if (p->type == C_VIEW && !(wlc_view_get_type(p->handle) & WLC_BIT_POPUP)) {
		// unactivate previous focus
		if (focused->type == C_VIEW) {
			wlc_view_set_state(focused->handle, WLC_BIT_ACTIVATED, false);
		}
		// activate current focus
		if (p->type == C_VIEW) {
			wlc_view_set_state(p->handle, WLC_BIT_ACTIVATED, true);
			// set focus if view_focus is unlocked
			if (!locked_view_focus) {
				wlc_view_focus(p->handle);
			}
		}
	}
}

void set_focused_container_for(swayc_t *a, swayc_t *c) {
	if (locked_container_focus || !c) {
		return;
	}
	swayc_t *find = c;
	// Ensure that a is an ancestor of c
	while (find != a && (find = find->parent)) {
		if (find == &root_container) {
			return;
		}
	}

	// Get workspace for c, get that workspaces current focused container.
	// if that focsued container is fullscreen dont change focus
	swayc_t *workspace = swayc_parent_by_type(c, C_WORKSPACE);
	swayc_t *focused = get_focused_view(workspace);
	if (swayc_is_fullscreen(focused)) {
		return;
	}

	// Check if we changing a parent container that will see chnage
	bool effective = true;
	while (find != &root_container) {
		if (find->parent->focused != find) {
			effective = false;
		}
		find = find->parent;
	}
	if (effective) {
		// Go to set_focused_container
		set_focused_container(c);
		return;
	}

	sway_log(L_DEBUG, "Setting focus for %p:%ld to %p:%ld",
		a, a->handle, c, c->handle);

	c->is_focused = true;
	swayc_t *p = c;
	while (p != a) {
		update_focus(p);
		p = p->parent;
		p->is_focused = false;
	}
}

swayc_t *get_focused_view(swayc_t *parent) {
	while (parent && parent->type != C_VIEW) {
		if (parent->type == C_WORKSPACE && parent->focused == NULL) {
			return parent;
		}
		parent = parent->focused;
	}
	if (parent == NULL) {
		return active_workspace;
	}
	return parent;
}
