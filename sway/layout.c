#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "log.h"
#include "list.h"
#include "config.h"
#include "container.h"
#include "workspace.h"
#include "focus.h"

swayc_t root_container;

void init_layout(void) {
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.children = create_list();
	root_container.handle = 0;
}

// Children of container functions

static int index_child(swayc_t *parent, swayc_t *child) {
	int i;
	for (i = 0; i < parent->children->length; ++i) {
		if (parent->children->items[i] == child) {
			break;
		}
	}
	return i;
}

void add_child(swayc_t *parent, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %dx%d) to %p (%d, %dx%d)", child, child->type,
		child->width, child->height, parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
	child->parent = parent;
	// set focus for this container
	if (parent->children->length == 1) {
		parent->focused = child;
		set_focused_container_for(parent, get_focused_view(parent));
	}
}

void add_floating(swayc_t *ws, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %dx%d) to %p (%d, %dx%d)", child, child->type,
		child->width, child->height, ws, ws->type, ws->width, ws->height);
	list_add(ws->floating, child);
	child->parent = ws;
	child->is_floating = true;
	if (!ws->focused) {
		ws->focused = child;
	}
}

swayc_t *add_sibling(swayc_t *sibling, swayc_t *child) {
	swayc_t *parent = sibling->parent;
	int i = index_child(parent, sibling);
	if (i == parent->children->length) {
		--i;
	}
	list_insert(parent->children, i+1, child);
	child->parent = parent;
	return child->parent;
}

swayc_t *replace_child(swayc_t *child, swayc_t *new_child) {
	swayc_t *parent = child->parent;
	if (parent == NULL) {
		return NULL;
	}
	int i = index_child(parent, child);
	parent->children->items[i] = new_child;
	new_child->parent = child->parent;

	if (child->parent->focused == child) {
		child->parent->focused = new_child;
	}
	child->parent = NULL;
	return parent;
}

swayc_t *remove_child(swayc_t *child) {
	int i;
	swayc_t *parent = child->parent;
	if (child->is_floating) {
		// Special case for floating views
		for (i = 0; i < parent->floating->length; ++i) {
			if (parent->floating->items[i] == child) {
				list_del(parent->floating, i);
				break;
			}
		}
		i = 0;
	} else {
		for (i = 0; i < parent->children->length; ++i) {
			if (parent->children->items[i] == child) {
				list_del(parent->children, i);
				break;
			}
		}
	}
	// Set focused to new container
	if (parent->focused == child) {
		if (parent->children->length > 0) {
			parent->focused = parent->children->items[i?i-1:0];
		} else {
			parent->focused = NULL;
		}
	}
	child->parent = NULL;
	return parent;
}

// Fitting functions
#define FIT_FUNC __attribute__((nonnull)) static void
FIT_FUNC _fit_view(swayc_t *view) {
	sway_log(L_DEBUG, "%s:%p: (%dx%d@%dx%d)", __func__, view,
			view->width, view->height, view->x, view->y);
	struct wlc_geometry geo;
	if (wlc_view_get_state(view->handle) & WLC_BIT_FULLSCREEN) {
		swayc_t *op = swayc_parent_by_type(view, C_OUTPUT);
		geo = (struct wlc_geometry){
			.origin = { .x = 0, .y = 0 },
			.size = { .w = op->width, .h = op->height }
		};
	} else {
		if (view->is_floating) {
			geo = (struct wlc_geometry){
				.origin = {
					.x = view->x,
					.y = view->y,
				},
					.size = {
					.w = view->width,
					.h = view->height,
				}
			};
		} else {
			geo = (struct wlc_geometry){
				.origin = {
					.x = view->x + view->gaps / 2,
					.y = view->y + view->gaps / 2,
				},
					.size = {
					.w = view->width - view->gaps,
					.h = view->height - view->gaps
				}
			};
		}
	}
	wlc_view_set_geometry(view->handle, 0, &geo);
}

FIT_FUNC _fit_container(swayc_t *container) {
	sway_log(L_DEBUG, "%s:%p: (%dx%d@%dx%d)", __func__, container,
			container->width, container->height, container->x, container->y);
	swayc_t **child = (swayc_t **)container->children->items;
	int i, len = container->children->length;
	// geometry
	int x = container->x;
	int y = container->y;
	int w = container->width;
	int h = container->height;
	// scaling
	double s = 0;
	switch (container->layout) {
	default:
	case L_HORIZ:
		// Find scaling amount required to fit children in parent
		for (i = 0; i < len; ++i, ++child) {
			int *prev_w = &(*child)->width;
			if (*prev_w <= 0) {
				*prev_w = w / (len > 1 ? len - 1 : 1);
			}
			s += *prev_w;
		}
		if (s < 0.001) {
			return;
		}
		s = w / s;
		child = (swayc_t **)container->children->items;
		for (i = 0; i < len; ++i, ++child) {
			// Set geometry
			(*child)->x = x;
			(*child)->y = y;
			// Scale width to fit in container
			(*child)->width *= s;
			(*child)->height = h;
			// Increment x offset
			x += (*child)->width;
			// Recursibly resize children, depending on its type
			((*child)->type == C_VIEW ? _fit_view : _fit_container)(*child);
		}
		return;

	case L_VERT:
		// Find scaling amount required to fit children in parent
		for (i = 0; i < len; ++i, ++child) {
			int *prev_h = &(*child)->height;
			if (*prev_h <= 0) {
				*prev_h = h / (len > 1 ? len - 1 : 1);
			}
			s += *prev_h;
		}
		if (s < 0.001) {
			return;
		}
		s = h / s;
		child = (swayc_t **)container->children->items;
		for (i = 0; i < len; ++i, ++child) {
			// Set geometry
			(*child)->x = x;
			(*child)->y = y;
			// Scale height to fit in container
			(*child)->height *= s;
			(*child)->width = w;
			// Increment y offset
			y += (*child)->height;
			// Recursibly resize children, depending on its type
			((*child)->type == C_VIEW ? _fit_view : _fit_container)(*child);
		}
		return;
	}
}

FIT_FUNC _fit_workspace(swayc_t *workspace) {
	workspace->x += workspace->gaps;
	workspace->y += workspace->gaps;
	workspace->width -= workspace->gaps * 2;
	workspace->height -= workspace->gaps * 2;
	sway_log(L_DEBUG, "%s:%p: (%dx%d@%dx%d)", __func__, workspace,
			workspace->width, workspace->height, workspace->x,workspace->y);
	// workspace is treated same as container for tiling
	_fit_container(workspace);
	// Handle floating containers
	swayc_t **child = (swayc_t **)workspace->floating->items;
	int i, len = workspace->floating->length;
	for (i = 0; i < len; ++i, ++child) {
		// Fit floating depending on view
		((*child)->type == C_VIEW ? _fit_view : _fit_container)(*child);
	}
}

FIT_FUNC _fit_output(swayc_t *output) {
	sway_log(L_DEBUG, "%s:%p: (%dx%d@%dx%d)", __func__, output, output->width,
			output->height, output->x,output->y);
	swayc_t **child = (swayc_t **)output->children->items;
	int i, len = output->children->length;
	int x = output->x;
	int y = output->y;
	int w = output->width;
	int h = output->height;
	for (i = 0; i < len; ++i, ++child) {
		// Set geometry with gaps
		(*child)->x = x;
		(*child)->y = y;
		(*child)->width = w;
		(*child)->height = h;
		// recursivly fit children
		_fit_workspace(*child);
	}
}

FIT_FUNC _fit_root(swayc_t *root) {
	sway_log(L_DEBUG, "%s:%p: (%dx%d@%dx%d)", __func__, root, root->width,
			root->height, root->x,root->y);
	swayc_t **child = (swayc_t **)root->children->items;
	int i, len = root->children->length;
	for (i = 0; i < len; ++i, ++child) {
		(*child)->x = 0;
		(*child)->y = 0;
		_fit_output(*child);
	}
}

FIT_FUNC fit_children_in(swayc_t *swayc)  {
	switch (swayc->type) {
	case C_ROOT: _fit_root(swayc); return;
	case C_WORKSPACE: swayc = swayc->parent;
	case C_OUTPUT: _fit_output(swayc); return;
	case C_VIEW: swayc = swayc->parent;
	case C_CONTAINER: _fit_container(swayc); return;
	default: sway_assert(false, "%s: invalid type", __func__);
	}
}
#undef FIT_FUNC

__attribute__((nonnull)) static wlc_handle order_children(swayc_t *swayc, wlc_handle top) {
	// return handle
	if (swayc->type == C_VIEW) {
		return swayc->handle;
	}
	// return if no focus
	if (swayc->focused == NULL) {
		return 0;
	}

	// TODO fix floating implementation.
	// Recurse and get handle of focused container
	wlc_handle bottom = order_children(swayc->focused, top);

	// TODO Used properly in workspace case
	wlc_handle topmost = bottom;
	(void) topmost;

	// Put handle below current top, or if there is no top, set it as bottom
	if (top) {
		wlc_view_send_below(bottom, top);
		sway_log(L_DEBUG,"putting %ld below %ld", bottom, top);
	} else {
		top = bottom;
	}

	// recurse for all children with the current bottom handle as their top.
	int i, len = swayc->children->length;
	swayc_t **child = (swayc_t **)swayc->children->items;
	for (i = 0; i < len; ++i, ++child) {
		// Skip over the focused child
		if (*child == swayc->focused) {
			continue;
		}
		// Update current top, send it below previous bottom, and set new bottom
		top = order_children(*child, bottom);
		wlc_view_send_below(top, bottom);
		sway_log(L_DEBUG,"putting %ld below %ld", bottom, top);
		bottom = top;
	}
	
	// TODO fix floating implementation.
	// If we are at a workspace, put floating containers above topmost
	if (swayc->type == C_WORKSPACE) {
		// TODO using the same hacky implementation as before.
		// send floating windows to front
		len = swayc->floating->length;
		child = (swayc_t **)swayc->floating->items;
		
		// chekc whether to send floating windows to the back or front
		swayc_t *focused = get_focused_view(swayc);

		bool tofront = swayc->parent->focused == swayc;
		// Send focused view to front or back if fullscreen depending on tofront
		if (!focused->is_floating && swayc_is_fullscreen(focused)) {
			if (tofront) {
				wlc_view_bring_to_front(focused->handle);
				sway_log(L_DEBUG,"bring %ld to front (fullscreen)", focused->handle);
				// we dont want to bring floating windows above fullscreen
				// window
				tofront = false;
			} else {
				wlc_view_send_to_back(focused->handle);
			}
		}
		for (i = 0; i < len; ++i, ++child) {
			if (tofront) {
				wlc_view_bring_to_front((*child)->handle);
				sway_log(L_DEBUG,"bring %ld to front (floating)", focused->handle);
			} else {
				wlc_view_send_to_back((*child)->handle);
				sway_log(L_DEBUG,"sending %ld to back (floating)", focused->handle);
			}
		}
	}
	// Return the bottom of this, which shall be the top of others,
	return bottom;
}

// Arrange layout

void arrange_windows(swayc_t *container, int width, int height) {
	fit_children_in(container);
	order_children(container, 0);
	layout_log(&root_container, 0);
	return;
}

