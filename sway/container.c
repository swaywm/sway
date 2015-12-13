#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>
#include <string.h>
#include "config.h"
#include "stringop.h"
#include "container.h"
#include "workspace.h"
#include "focus.h"
#include "layout.h"
#include "input_state.h"
#include "log.h"

#define ASSERT_NONNULL(PTR) \
	sway_assert (PTR, #PTR "must be non-null")

static swayc_t *new_swayc(enum swayc_types type) {
	swayc_t *c = calloc(1, sizeof(swayc_t));
	c->handle = -1;
	c->gaps = -1;
	c->layout = L_NONE;
	c->type = type;
	if (type != C_VIEW) {
		c->children = create_list();
	}
	return c;
}

static void free_swayc(swayc_t *cont) {
	if (!ASSERT_NONNULL(cont)) {
		return;
	}
	if (cont->children) {
		// remove children until there are no more, free_swayc calls
		// remove_child, which removes child from this container
		while (cont->children->length) {
			free_swayc(cont->children->items[0]);
		}
		list_free(cont->children);
	}
	if (cont->floating) {
		while (cont->floating->length) {
			free_swayc(cont->floating->items[0]);
		}
		list_free(cont->floating);
	}
	if (cont->parent) {
		remove_child(cont);
	}
	if (cont->name) {
		free(cont->name);
	}
	if (cont->class) {
		free(cont->class);
	}
	if (cont->app_id) {
		free(cont->app_id);
	}
	free(cont);
}

// New containers

swayc_t *new_output(wlc_handle handle) {
	const struct wlc_size *size = wlc_output_get_resolution(handle);
	const char *name = wlc_output_get_name(handle);
	// Find current outputs to see if this already exists
	{
		int i, len = root_container.children->length;
		for (i = 0; i < len; ++i) {
			swayc_t *op = root_container.children->items[i];
			const char *op_name = op->name;
			if (op_name && name && strcmp(op_name, name) == 0) {
				sway_log(L_DEBUG, "restoring output %lu:%s", handle, op_name);
				return op;
			}
		}
	}

	sway_log(L_DEBUG, "New output %lu:%s", handle, name);

	struct output_config *oc = NULL;
	int i;
	for (i = 0; i < config->output_configs->length; ++i) {
		struct output_config *cur = config->output_configs->items[i];
		if (strcasecmp(name, cur->name) == 0) {
			sway_log(L_DEBUG, "Matched output config for %s", name);
			oc = cur;
			break;
		}
		if (strcasecmp("*", cur->name) == 0) {
			sway_log(L_DEBUG, "Matched wildcard output config for %s", name);
			oc = cur;
			break;
		}
	}

	if (oc && !oc->enabled) {
		return NULL;
	}

	swayc_t *output = new_swayc(C_OUTPUT);
	output->handle = handle;
	output->name = name ? strdup(name) : NULL;
	output->width = size->w;
	output->height = size->h;

	apply_output_config(oc, output);
	add_child(&root_container, output);

	// Create workspace
	char *ws_name = NULL;
	if (name) {
		for (i = 0; i < config->workspace_outputs->length; ++i) {
			struct workspace_output *wso = config->workspace_outputs->items[i];
			if (strcasecmp(wso->output, name) == 0) {
				sway_log(L_DEBUG, "Matched workspace to output: %s for %s", wso->workspace, wso->output);
				// Check if any other workspaces are using this name
				if (workspace_by_name(wso->workspace)) {
					sway_log(L_DEBUG, "But it's already taken");
					break;
				}
				sway_log(L_DEBUG, "So we're going to use it");
				ws_name = strdup(wso->workspace);
				break;
			}
		}
	}
	if (!ws_name) {
		ws_name = workspace_next_name();
	}

	// create and initilize default workspace
	swayc_t *ws = new_workspace(output, ws_name);
	ws->is_focused = true;

	free(ws_name);
	
	return output;
}

swayc_t *new_workspace(swayc_t *output, const char *name) {
	if (!ASSERT_NONNULL(output)) {
		return NULL;
	}
	sway_log(L_DEBUG, "Added workspace %s for output %u", name, (unsigned int)output->handle);
	swayc_t *workspace = new_swayc(C_WORKSPACE);

	// TODO: default_layout
	if (config->default_layout != L_NONE) {
		workspace->layout = config->default_layout;
	} else if (config->default_orientation != L_NONE) {
		workspace->layout = config->default_orientation;
	} else if (output->width >= output->height) {
		workspace->layout = L_HORIZ;
	} else {
		workspace->layout = L_VERT;
	}
	workspace->x = output->x;
	workspace->y = output->y;
	workspace->width = output->width;
	workspace->height = output->height;
	workspace->name = strdup(name);
	workspace->visible = false;
	workspace->floating = create_list();

	add_child(output, workspace);
	return workspace;
}

swayc_t *new_container(swayc_t *child, enum swayc_layouts layout) {
	if (!ASSERT_NONNULL(child)
			&& !sway_assert(!child->is_floating, "cannot create container around floating window")) {
		return NULL;
	}
	swayc_t *cont = new_swayc(C_CONTAINER);

	sway_log(L_DEBUG, "creating container %p around %p", cont, child);

	cont->layout = layout;
	cont->width = child->width;
	cont->height = child->height;
	cont->x = child->x;
	cont->y = child->y;
	cont->visible = child->visible;

	/* Container inherits all of workspaces children, layout and whatnot */
	if (child->type == C_WORKSPACE) {
		swayc_t *workspace = child;
		// reorder focus
		cont->focused = workspace->focused;
		workspace->focused = cont;
		// set all children focu to container
		int i;
		for (i = 0; i < workspace->children->length; ++i) {
			((swayc_t *)workspace->children->items[i])->parent = cont;
		}
		// Swap children
		list_t  *tmp_list  = workspace->children;
		workspace->children = cont->children;
		cont->children = tmp_list;
		// add container to workspace chidren
		add_child(workspace, cont);
		// give them proper layouts
		cont->layout = workspace->layout;
		workspace->layout = layout;
		set_focused_container_for(workspace, get_focused_view(workspace));
	} else { // Or is built around container
		swayc_t *parent = replace_child(child, cont);
		if (parent) {
			add_child(cont, child);
		}
	}
	return cont;
}

swayc_t *new_view(swayc_t *sibling, wlc_handle handle) {
	if (!ASSERT_NONNULL(sibling)) {
		return NULL;
	}
	const char *title = wlc_view_get_title(handle);
	swayc_t *view = new_swayc(C_VIEW);
	sway_log(L_DEBUG, "Adding new view %lu:%s to container %p %d",
		handle, title, sibling, sibling ? sibling->type : 0);
	// Setup values
	view->handle = handle;
	view->name = title ? strdup(title) : NULL;
	const char *class = wlc_view_get_class(handle);
	view->class = class ? strdup(class) : NULL;
	const char *app_id = wlc_view_get_app_id(handle);
	view->app_id = app_id ? strdup(app_id) : NULL;
	view->visible = true;
	view->is_focused = true;
	view->sticky = false;
	// Setup geometry
	const struct wlc_geometry* geometry = wlc_view_get_geometry(handle);
	view->width = 0;
	view->height = 0;
	view->desired_width = geometry->size.w;
	view->desired_height = geometry->size.h;

	view->is_floating = false;

	if (sibling->type == C_WORKSPACE) {
		// Case of focused workspace, just create as child of it
		add_child(sibling, view);
	} else {
		// Regular case, create as sibling of current container
		add_sibling(sibling, view);
	}
	return view;
}

swayc_t *new_floating_view(wlc_handle handle) {
	if (swayc_active_workspace() == NULL) {
		return NULL;
	}
	const char *title = wlc_view_get_title(handle);
	swayc_t *view = new_swayc(C_VIEW);
	sway_log(L_DEBUG, "Adding new view %lu:%x:%s as a floating view",
		handle, wlc_view_get_type(handle), title);
	// Setup values
	view->handle = handle;
	view->name = title ? strdup(title) : NULL;
	const char *class = wlc_view_get_class(handle);
	view->class = class ? strdup(class) : NULL;
	const char *app_id = wlc_view_get_app_id(handle);
	view->app_id = app_id ? strdup(app_id) : NULL;
	view->visible = true;
	view->sticky = false;

	// Set the geometry of the floating view
	const struct wlc_geometry* geometry = wlc_view_get_geometry(handle);

	// give it requested geometry, but place in center
	view->x = (swayc_active_workspace()->width - geometry->size.w) / 2;
	view->y = (swayc_active_workspace()->height- geometry->size.h) / 2;
	view->width = geometry->size.w;
	view->height = geometry->size.h;

	view->desired_width = view->width;
	view->desired_height = view->height;

	view->is_floating = true;

	// Case of focused workspace, just create as child of it
	list_add(swayc_active_workspace()->floating, view);
	view->parent = swayc_active_workspace();
	if (swayc_active_workspace()->focused == NULL) {
		set_focused_container_for(swayc_active_workspace(), view);
	}
	return view;
}

// Destroy container

swayc_t *destroy_output(swayc_t *output) {
	if (!ASSERT_NONNULL(output)) {
		return NULL;
	}
	if (output->children->length > 0) {
		// TODO save workspaces when there are no outputs.
		// TODO also check if there will ever be no outputs except for exiting
		// program
		if (root_container.children->length > 1) {
			int p = root_container.children->items[0] == output;
			// Move workspace from this output to another output
			while (output->children->length) {
				swayc_t *child = output->children->items[0];
				remove_child(child);
				add_child(root_container.children->items[p], child);
			}
			update_visibility(root_container.children->items[p]);
			arrange_windows(root_container.children->items[p], -1, -1);
		}
	}
	sway_log(L_DEBUG, "OUTPUT: Destroying output '%lu'", output->handle);
	free_swayc(output);
	return &root_container;
}

swayc_t *destroy_workspace(swayc_t *workspace) {
	if (!ASSERT_NONNULL(workspace)) {
		return NULL;
	}
	
	// Do not destroy this if it's the last workspace on this output
	swayc_t *output = swayc_parent_by_type(workspace, C_OUTPUT);
	if (output && output->children->length == 1) {
		return NULL;
	}

	swayc_t *parent = workspace->parent;
	// destroy the WS if there are no children
	if (workspace->children->length == 0 && workspace->floating->length == 0) {
		sway_log(L_DEBUG, "destroying workspace '%s'", workspace->name);
	} else {
		// Move children to a different workspace on this output
		swayc_t *new_workspace = NULL;
		int i;
		for(i = 0; i < output->children->length; i++) {
			if(output->children->items[i] != workspace) {
				break;
			}
		}
		new_workspace = output->children->items[i];

		sway_log(L_DEBUG, "moving children to different workspace '%s' -> '%s'",
			workspace->name, new_workspace->name);

		for(i = 0; i < workspace->children->length; i++) {
			move_container_to(workspace->children->items[i], new_workspace);
		}

		for(i = 0; i < workspace->floating->length; i++) {
			move_container_to(workspace->floating->items[i], new_workspace);
		}
	}

	free_swayc(workspace);
	return parent;
}

swayc_t *destroy_container(swayc_t *container) {
	if (!ASSERT_NONNULL(container)) {
		return NULL;
	}
	while (container->children->length == 0 && container->type == C_CONTAINER) {
		sway_log(L_DEBUG, "Container: Destroying container '%p'", container);
		swayc_t *parent = container->parent;
		free_swayc(container);
		container = parent;
	}
	return container;
}

swayc_t *destroy_view(swayc_t *view) {
	if (!ASSERT_NONNULL(view)) {
		return NULL;
	}
	sway_log(L_DEBUG, "Destroying view '%p'", view);
	swayc_t *parent = view->parent;
	free_swayc(view);

	// Destroy empty containers
	if (parent->type == C_CONTAINER) {
		return destroy_container(parent);
	}
	return parent;
}

// Container lookup


swayc_t *swayc_by_test(swayc_t *container, bool (*test)(swayc_t *view, void *data), void *data) {
	if (!container->children) {
		return NULL;
	}
	// Special case for checking floating stuff
	int i;
	if (container->type == C_WORKSPACE) {
		for (i = 0; i < container->floating->length; ++i) {
			swayc_t *child = container->floating->items[i];
			if (test(child, data)) {
				return child;
			}
		}
	}
	for (i = 0; i < container->children->length; ++i) {
		swayc_t *child = container->children->items[i];
		if (test(child, data)) {
			return child;
		} else {
			swayc_t *res = swayc_by_test(child, test, data);
			if (res) {
				return res;
			}
		}
	}
	return NULL;
}

static bool test_name(swayc_t *view, void *data) {
	if (!view || !view->name) {
		return false;
	}
	return strcmp(view->name, data) == 0;
}

swayc_t *swayc_by_name(const char *name) {
	return swayc_by_test(&root_container, test_name, (void *)name);
}

swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types type) {
	if (!ASSERT_NONNULL(container)) {
		return NULL;
	}
	if (!sway_assert(type < C_TYPES && type >= C_ROOT, "invalid type")) {
		return NULL;
	}
	do {
		container = container->parent;
	} while (container && container->type != type);
	return container;
}

swayc_t *swayc_parent_by_layout(swayc_t *container, enum swayc_layouts layout) {
	if (!ASSERT_NONNULL(container)) {
		return NULL;
	}
	if (!sway_assert(layout < L_LAYOUTS && layout >= L_NONE, "invalid layout")) {
		return NULL;
	}
	do {
		container = container->parent;
	} while (container && container->layout != layout);
	return container;
}

swayc_t *swayc_focus_by_type(swayc_t *container, enum swayc_types type) {
	if (!ASSERT_NONNULL(container)) {
		return NULL;
	}
	if (!sway_assert(type < C_TYPES && type >= C_ROOT, "invalid type")) {
		return NULL;
	}
	do {
		container = container->focused;
	} while (container && container->type != type);
	return container;
}

swayc_t *swayc_focus_by_layout(swayc_t *container, enum swayc_layouts layout) {
	if (!ASSERT_NONNULL(container)) {
		return NULL;
	}
	if (!sway_assert(layout < L_LAYOUTS && layout >= L_NONE, "invalid layout")) {
		return NULL;
	}
	do {
		container = container->focused;
	} while (container && container->layout != layout);
	return container;
}


static swayc_t *_swayc_by_handle_helper(wlc_handle handle, swayc_t *parent) {
	if (!parent || !parent->children) {
		return NULL;
	}
	int i, len;
	swayc_t **child;
	if (parent->type == C_WORKSPACE) {
		len = parent->floating->length;
		child = (swayc_t **)parent->floating->items;
		for (i = 0; i < len; ++i, ++child) {
			if ((*child)->handle == handle) {
				return *child;
			}
		}
	}

	len = parent->children->length;
	child = (swayc_t**)parent->children->items;
	for (i = 0; i < len; ++i, ++child) {
		if ((*child)->handle == handle) {
			return *child;
		} else {
			swayc_t *res;
			if ((res = _swayc_by_handle_helper(handle, *child))) {
				return res;
			}
		}
	}
	return NULL;
}

swayc_t *swayc_by_handle(wlc_handle handle) {
	return _swayc_by_handle_helper(handle, &root_container);
}

swayc_t *swayc_active_output(void) {
	return root_container.focused;
}

swayc_t *swayc_active_workspace(void) {
	return root_container.focused ? root_container.focused->focused : NULL;
}

swayc_t *swayc_active_workspace_for(swayc_t *cont) {
	if (!cont) {
		return NULL;
	}
	switch (cont->type) {
	case C_ROOT:
		cont = cont->focused;
		/* Fallthrough */

	case C_OUTPUT:
		cont = cont ? cont->focused : NULL;
		/* Fallthrough */

	case C_WORKSPACE:
		return cont;

	default:
		return swayc_parent_by_type(cont, C_WORKSPACE);
	}
}

static bool pointer_test(swayc_t *view, void *_origin) {
	const struct wlc_point *origin = _origin;
	// Determine the output that the view is under
	swayc_t *parent = swayc_parent_by_type(view, C_OUTPUT);
	if (origin->x >= view->x && origin->y >= view->y
		&& origin->x < view->x + view->width && origin->y < view->y + view->height
		&& view->visible && parent == root_container.focused) {
		return true;
	}
	return false;
}

swayc_t *container_under_pointer(void) {
	// root.output->workspace
	if (!root_container.focused || !root_container.focused->focused) {
		return NULL;
	}
	swayc_t *lookup = root_container.focused->focused;
	// Case of empty workspace
	if (lookup->children == 0) {
		return NULL;
	}
	struct wlc_point origin;
	wlc_pointer_get_position(&origin);
	while (lookup->type != C_VIEW) {
		int i;
		int len;
		// if tabbed/stacked go directly to focused container, otherwise search
		// children
		if (lookup->layout == L_TABBED || lookup->layout == L_STACKED) {
			lookup = lookup->focused;
			continue;
		}
		// if workspace, search floating
		if (lookup->type == C_WORKSPACE) {
			i = len = lookup->floating->length;
			bool got_floating = false;
			while (--i > -1) {
				if (pointer_test(lookup->floating->items[i], &origin)) {
					lookup = lookup->floating->items[i];
					got_floating = true;
					break;
				}
			}
			if (got_floating) {
				continue;
			}
		}
		// search children
		len = lookup->children->length;
		for (i = 0; i < len; ++i) {
			if (pointer_test(lookup->children->items[i], &origin)) {
				lookup = lookup->children->items[i];
				break;
			}
		}
		// when border and titles are done, this could happen
		if (i == len) {
			break;
		}
	}
	return lookup;
}

// Container information

bool swayc_is_fullscreen(swayc_t *view) {
	return view && view->type == C_VIEW && (wlc_view_get_state(view->handle) & WLC_BIT_FULLSCREEN);
}

bool swayc_is_active(swayc_t *view) {
	return view && view->type == C_VIEW && (wlc_view_get_state(view->handle) & WLC_BIT_ACTIVATED);
}

bool swayc_is_parent_of(swayc_t *parent, swayc_t *child) {
	while (child != &root_container) {
		child = child->parent;
		if (child == parent) {
			return true;
		}
	}
	return false;
}

bool swayc_is_child_of(swayc_t *child, swayc_t *parent) {
	return swayc_is_parent_of(parent, child);
}

int swayc_gap(swayc_t *container) {
	if (container->type == C_VIEW) {
		return container->gaps >= 0 ? container->gaps : config->gaps_inner;
	} else if (container->type == C_WORKSPACE) {
		return container->gaps >= 0 ? container->gaps : config->gaps_outer;
	} else {
		return 0;
	}
}

// Mapping

void container_map(swayc_t *container, void (*f)(swayc_t *view, void *data), void *data) {
	if (container) {
		f(container, data);
		int i;
		if (container->children)  {
			for (i = 0; i < container->children->length; ++i) {
				swayc_t *child = container->children->items[i];
				container_map(child, f, data);
			}
		}
		if (container->floating) {
			for (i = 0; i < container->floating->length; ++i) {
				swayc_t *child = container->floating->items[i];
				container_map(child, f, data);
			}
		}
	}
}

void update_visibility_output(swayc_t *container, wlc_handle output) {
	// Inherit visibility
	swayc_t *parent = container->parent;
	container->visible = parent->visible;
	// special cases where visibility depends on focus
	if (parent->type == C_OUTPUT
			|| parent->layout == L_TABBED
			|| parent->layout == L_STACKED) {
		container->visible = parent->focused == container;
	}
	// Set visibility and output for view
	if (container->type == C_VIEW) {
		wlc_view_set_output(container->handle, output);
		wlc_view_set_mask(container->handle, container->visible ? VISIBLE : 0);
		if (container->visible) {
			wlc_view_bring_to_front(container->handle);
		} else {
			wlc_view_send_to_back(container->handle);
		}
	}
	// Update visibility for children
	else {
		if (container->children) {
			int i, len = container->children->length;
			for (i = 0; i < len; ++i) {
				update_visibility_output(container->children->items[i], output);
			}
		}
		if (container->floating) {
			int i, len = container->floating->length;
			for (i = 0; i < len; ++i) {
				update_visibility_output(container->floating->items[i], output);
			}
		}
	}
}

void update_visibility(swayc_t *container) {
	if (!container) return;
	switch (container->type) {
	case C_ROOT:
		container->visible = true;
		if (container->children) {
			int i, len = container->children->length;
			for (i = 0; i < len; ++i) {
				update_visibility(container->children->items[i]);
			}
		}
		return;

	case C_OUTPUT:
		container->visible = true;
		if (container->children) {
			int i, len = container->children->length;
			for (i = 0; i < len; ++i) {
				update_visibility_output(container->children->items[i], container->handle);
			}
		}
		return;

	default:
		{
			swayc_t *op = swayc_parent_by_type(container, C_OUTPUT);
			update_visibility_output(container, op->handle);
		}
	}
}

void set_gaps(swayc_t *view, void *_data) {
	int *data = _data;
	if (!ASSERT_NONNULL(view)) {
		return;
	}
	if (view->type == C_WORKSPACE || view->type == C_VIEW) {
		view->gaps = *data;
	}
}

void add_gaps(swayc_t *view, void *_data) {
	int *data = _data;
	if (!ASSERT_NONNULL(view)) {
		return;
	}
	if (view->type == C_WORKSPACE || view->type == C_VIEW) {
		if ((view->gaps += *data) < 0) {
			view->gaps = 0;
		}
	}
}
