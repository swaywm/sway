#include <wlc/wlc.h>
#include "sway/focus.h"
#include "sway/workspace.h"
#include "sway/layout.h"
#include "sway/config.h"
#include "sway/extensions.h"
#include "sway/input_state.h"
#include "sway/ipc-server.h"
#include "sway/border.h"
#include "log.h"

bool locked_container_focus = false;
bool suspend_workspace_cleanup = false;

// switches parent focus to c. will switch it accordingly
static void update_focus(swayc_t *c) {
	// Handle if focus switches
	swayc_t *parent = c->parent;
	if (!parent) return;
	if (parent->focused != c) {
		// Get previous focus
		swayc_t *prev = parent->focused;
		// Set new focus
		parent->focused = c;

		switch (c->type) {
		// Shouldn't happen
		case C_ROOT: return;

		// Case where output changes
		case C_OUTPUT:
			wlc_output_focus(c->handle);
			break;

		// Case where workspace changes
		case C_WORKSPACE:
			if (prev) {
				ipc_event_workspace(prev, c, "focus");

				// if the old workspace has no children, destroy it
				if(prev->children->length == 0 && prev->floating->length == 0 && !suspend_workspace_cleanup) {
					destroy_workspace(prev);
				} else {
					// update visibility of old workspace
					update_visibility(prev);
				}
			}
			// Update visibility of newly focused workspace
			update_visibility(c);
			break;

		default:
		case C_VIEW:
		case C_CONTAINER:
			break;
		}
	}
}

bool move_focus(enum movement_direction direction) {
	swayc_t *old_view = get_focused_container(&root_container);
	swayc_t *new_view = get_swayc_in_direction(old_view, direction);
	if (!new_view) {
		return false;
	} else if (new_view->type == C_ROOT) {
		sway_log(L_DEBUG, "Not setting focus above the workspace level");
		return false;
	} else if (new_view->type == C_OUTPUT) {
		return set_focused_container(swayc_active_workspace_for(new_view));
	} else if (direction == MOVE_PARENT || direction == MOVE_CHILD) {
		return set_focused_container(new_view);
	} else if (config->mouse_warping) {
		swayc_t *old_op = old_view->type == C_OUTPUT ?
			old_view : swayc_parent_by_type(old_view, C_OUTPUT);
		swayc_t *focused = get_focused_view(new_view);
		if (set_focused_container(focused)) {
			if (old_op != swayc_active_output() && focused && focused->type == C_VIEW) {
				center_pointer_on(focused);
			}
			return true;
		}
	} else {
		return set_focused_container(get_focused_view(new_view));
	}
	return false;
}

swayc_t *get_focused_container(swayc_t *parent) {
	if (!parent) {
		return swayc_active_workspace();
	}
	while (!parent->is_focused && parent->focused) {
		parent = parent->focused;
	}
	return parent;
}

bool set_focused_container(swayc_t *c) {
	if (locked_container_focus || !c || !c->parent) {
		return false;
	}
	swayc_t *active_ws = swayc_active_workspace();
	int active_ws_child_count = 0;
	if (active_ws) {
		active_ws_child_count = active_ws->children->length + active_ws->floating->length;
	}

	swayc_t *workspace = swayc_active_workspace_for(c);
	swayc_t *focused = get_focused_container(&root_container);

	if (swayc_is_fullscreen(get_focused_container(workspace))) {
		// if switching to a workspace with a fullscreen view,
		// focus on the fullscreen view
		c = get_focused_container(workspace);
	}

	swayc_log(L_DEBUG, c, "Setting focus to %p:%" PRIuPTR, c, c->handle);

	if (c->type == C_VIEW) {
		// dispatch a window event
		ipc_event_window(c, "focus");
	}

	// update the global pointer
	current_focus = c;

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

	if (!(wlc_view_get_type(p->handle) & WLC_BIT_POPUP)) {
		if (focused->type == C_VIEW) {
			wlc_view_set_state(focused->handle, WLC_BIT_ACTIVATED, false);
		}
		update_container_border(focused);
		if (c->type == C_VIEW) {
			wlc_view_set_state(c->handle, WLC_BIT_ACTIVATED, true);
		}
		if (!desktop_shell.is_locked) {
			// If the system is locked, we do everything _but_ actually setting
			// focus. This includes making our internals think that this view is
			// focused.
			wlc_view_focus(c->handle);
		}
		if (c->parent->layout != L_TABBED && c->parent->layout != L_STACKED) {
			update_container_border(c);
		}

		swayc_t *parent = swayc_tabbed_stacked_ancestor(c);
		if (parent != NULL) {
			arrange_backgrounds();
			arrange_windows(parent, -1, -1);
		}
	}

	if (active_ws != workspace) {
		// active_ws might have been destroyed by now
		// (focus swap away from empty ws = destroy ws)
		if (active_ws_child_count == 0) {
			active_ws = NULL;
		}

		ipc_event_workspace(active_ws, workspace, "focus");
	}
	return true;
}

bool set_focused_container_for(swayc_t *a, swayc_t *c) {
	if (locked_container_focus || !c) {
		return false;
	}
	swayc_t *find = c;
	while (find != a && (find = find->parent)) {
		if (find == &root_container) {
			return false;
		}
	}

	// Get workspace for c, get that workspaces current focused container.
	swayc_t *workspace = swayc_active_workspace_for(c);
	swayc_t *focused = get_focused_view(workspace);
	// if the workspace we are changing focus to has a fullscreen view return
	if (swayc_is_fullscreen(focused) && c != focused) {
		return false;
	}

	// Check if we are changing a parent container that will see change
	bool effective = true;
	while (find != &root_container) {
		if (find->parent->focused != find) {
			effective = false;
		}
		find = find->parent;
	}
	if (effective) {
		// Go to set_focused_container
		return set_focused_container(c);
	}

	sway_log(L_DEBUG, "Setting focus for %p:%" PRIuPTR " to %p:%" PRIuPTR,
		a, a->handle, c, c->handle);

	c->is_focused = true;
	swayc_t *p = c;
	while (p != a) {
		update_focus(p);
		p = p->parent;
		p->is_focused = false;
	}
	return true;
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

swayc_t *get_focused_view_include_floating(swayc_t *parent) {
	swayc_t *c = parent;
	swayc_t *f = NULL;

	while (c && c->type != C_VIEW) {
		if (c->type == C_WORKSPACE && c->focused == NULL) {
			return ((f = get_focused_float(c))) ? f : c;
		}

		c = c->focused;
	}

	if (c == NULL) {
		c = swayc_active_workspace_for(parent);
	}

	return c;
}
