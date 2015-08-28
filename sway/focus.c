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
		// Get previous focus
		swayc_t *prev = parent->focused;
		// Set new focus
		parent->focused = c;
		switch (c->type) {
		// Shouldnt happen
		case C_ROOT: return;

		// Case where output changes
		case C_OUTPUT:
			wlc_output_focus(c->handle);
			break;

		// Case where workspace changes
		case C_WORKSPACE:
			if (prev) {
				// update visibility of old workspace
				update_visibility(prev);
				destroy_workspace(prev);
			}
			// Update visibility of newly focused workspace
			update_visibility(c);
			break;

		default:
		case C_VIEW:
		case C_CONTAINER:
			// TODO whatever to do when container changes
			// for example, stacked and tabbing change stuff.
			break;
		}
	}
}

bool move_focus(enum movement_direction direction) {
	swayc_t *view = get_focused_container(&root_container);
	view = get_swayc_in_direction(view, direction);
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
	if (!parent) {
		return swayc_active_workspace();
	}
	// get focusde container
	while (!parent->is_focused && parent->focused) {
		parent = parent->focused;
	}
	return parent;
}

void set_focused_container(swayc_t *c) {
	if (locked_container_focus || !c) {
		return;
	}
	sway_log(L_DEBUG, "Setting focus to %p:%ld", c, c->handle);

	// Get workspace for c, get that workspaces current focused container.
	swayc_t *workspace = swayc_active_workspace_for(c);
	swayc_t *focused = get_focused_view(workspace);
	// if the workspace we are changing focus to has a fullscreen view return
	if (swayc_is_fullscreen(focused) && focused != c) {
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
	swayc_t *workspace = swayc_active_workspace_for(c);
	swayc_t *focused = get_focused_view(workspace);
	// if the workspace we are changing focus to has a fullscreen view return
	if (swayc_is_fullscreen(focused) && c != focused) {
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
	swayc_t *c = parent;
	while (c && c->type != C_VIEW) {
		if (c->type == C_WORKSPACE && c->focused == NULL) {
			return c;
		}
		c = c->focused;
	}
	if (c == NULL) {
		c = swayc_active_workspace_for(parent);
	}
	return c;
}

swayc_t *get_focused_float(swayc_t *ws) {
	if(!sway_assert(ws->type == C_WORKSPACE, "must be of workspace type")) {
		ws = swayc_active_workspace();
	}
	if (ws->floating->length) {
		return ws->floating->items[ws->floating->length - 1];
	}
	return NULL;
}
