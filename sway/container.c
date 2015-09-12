#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>
#include <pcre.h>
#include "config.h"
#include "container.h"
#include "workspace.h"
#include "focus.h"
#include "layout.h"
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
	free(cont);
}

// New containers

swayc_t *new_output(wlc_handle handle) {
	const char *name = wlc_output_get_name(handle);
	int i, len;

	// Find current outputs to see if this already exists
	if (name) {
		len = root_container.children->length;
		for (i = 0; i < len; ++i) {
			swayc_t *op = root_container.children->items[i];
			if (strcmp(op->name, name) == 0) {
				sway_log(L_DEBUG, "restoring output %lu:%s", handle, op->name);
				return op;
			}
		}
	} else {
		sway_log(L_ERROR, "Output has no given name");
		return NULL;
	}

	sway_log(L_DEBUG, "Adding output %lu:%s", handle, name);

	// Find output config
	struct output_config *oc = NULL;
	len = config->output_configs->length;
	for (i = 0; i < len; ++i) {
		oc = config->output_configs->items[i];
		if (strcasecmp(name, oc->name) == 0) {
			sway_log(L_DEBUG, "Matched output config for %s", name);
			break;
		}
		oc = NULL;
	}

	if (oc && !oc->enabled) {
		return NULL;
	}

	swayc_t *output = new_swayc(C_OUTPUT);
	output->handle = handle;
	output->name = name ? strdup(name) : NULL;

	if (oc) {
		// Set output width/height
		if (oc->width > 0 && oc->height > 0) {
			output->width = oc->width;
			output->height = oc->height;
			struct wlc_size geo = { .w = oc->width, .h = oc->height};
			wlc_output_set_resolution(handle, &geo);
		} else {
			struct wlc_size geo = *wlc_output_get_resolution(handle);
			output->width = geo.w;
			output->height = geo.h;
		}
		// find position in config or find where it should go
		// TODO more intelligent method
		if (oc->x > 0 && oc->y > 0) {
			output->x = oc->x;
			output->y = oc->y;
		} else {
			unsigned int x = 0;
			len = root_container.children->length;
			for (i = 0; i < len; ++i) {
				swayc_t *c = root_container.children->items[i];
				if (c->type == C_OUTPUT) {
					unsigned int cx = c->width + c->x;
					if (cx > x) {
						x = cx;
					}
				}
			}
			output->x = x;
		}
	}
	// Add as child to root
	add_child(&root_container, output);

	// create and initilize default workspace
	swayc_t *ws = new_workspace(output, NULL);
	ws->is_focused = true;

	return output;
}

swayc_t *new_workspace(swayc_t *output, const char *name) {
	swayc_t *ws = NULL;
	struct workspace_output *wsop;
	if (name) {
		// Find existing workspace with same name.
		// or workspace found by special name
		if ((ws = workspace_by_name(name))) {
			return ws;
		}
		// Find matching output from config
		if (!output) {
			if ((wsop = wsop_find_workspace(name))) {
				int i, len = root_container.children->length;
				for (i = 0; i < len; ++i) {
					swayc_t *op = root_container.children->items[i];
					if (strcasecmp(op->name, wsop->output) == 0) {
						output = op;
						goto find_wsop_end;
					}
				}
			}
			// Set output to active_output if there is no output.
			output = swayc_active_output();
			find_wsop_end:;
		}
	} else {
		// No name or output, use active_output
		if (!output) {
			output = swayc_active_output();
		}
		// search for available output name
		if (!(name = workspace_output_open_name(output))) {
			// otherwise just use simple next name
			name = workspace_next_name();
		}
	}
	swayc_t *workspace = new_swayc(C_WORKSPACE);
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
	view->visible = true;
	view->is_focused = true;
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
	view->visible = true;

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
	// NOTE: This is called from elsewhere without checking children length
	// TODO move containers to other workspaces?
	// for now just dont delete
	
	// Do not destroy this if it's the last workspace on this output
	swayc_t *output = swayc_parent_by_type(workspace, C_OUTPUT);
	if (output && output->children->length == 1) {
		return NULL;
	}

	// Do not destroy if there are children
	if (workspace->children->length == 0 && workspace->floating->length == 0) {
		sway_log(L_DEBUG, "destroying workspace '%s'", workspace->name);
		swayc_t *parent = workspace->parent;
		free_swayc(workspace);
		return parent;
	}
	return NULL;
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


swayc_t *swayc_by_test_r(swayc_t *container, swayc_test_func test, void *data) {
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
			swayc_t *res = swayc_by_test_r(child, test, data);
			if (res) {
				return res;
			}
		}
	}
	return NULL;
}
swayc_t *swayc_by_test(swayc_test_func test, void *data) {
	return swayc_by_test_r(&root_container, test, data);
}

void swayc_map_r(swayc_t *container, swayc_map_func f, void *data) {
	if (container) {
		f(container, data);
		int i;
		if (container->children)  {
			for (i = 0; i < container->children->length; ++i) {
				swayc_t *child = container->children->items[i];
				swayc_map_r(child, f, data);
			}
		}
		if (container->floating) {
			for (i = 0; i < container->floating->length; ++i) {
				swayc_t *child = container->floating->items[i];
				swayc_map_r(child, f, data);
			}
		}
	}
}
void swayc_map(swayc_map_func f, void *data) {
	swayc_map_r(&root_container, f, data);
}

void swayc_map_by_test_r(swayc_t *container,
		swayc_map_func func, swayc_test_func test,
		void *funcdata, void *testdata) {
	if (container) {
		if (test(container, testdata)) {
			func(container, funcdata);
		}
		int i;
		if (container->children)  {
			for (i = 0; i < container->children->length; ++i) {
				swayc_t *child = container->children->items[i];
				swayc_map_by_test_r(child, func, test, funcdata, testdata);
			}
		}
		if (container->floating) {
			for (i = 0; i < container->floating->length; ++i) {
				swayc_t *child = container->floating->items[i];
				swayc_map_by_test_r(child, func, test, funcdata, testdata);
			}
		}
	}
}
void swayc_map_by_test(
		swayc_map_func func, swayc_test_func test,
		void *funcdata, void *testdata) {
	swayc_map_by_test_r(&root_container, func, test, funcdata, testdata);
}



// Map functions
void set_gaps(swayc_t *view, void *_data) {
	int *data = _data;
	if (view->type == C_WORKSPACE || view->type == C_VIEW) {
		view->gaps = *data;
	}
}

void add_gaps(swayc_t *view, void *_data) {
	int *data = _data;
	if (view->type == C_WORKSPACE || view->type == C_VIEW) {
		if ((view->gaps += *data) < 0) {
			view->gaps = 0;
		}
	}
}

// Test functions
bool test_name(swayc_t *view, void *data) {
	return view->name && strcmp(view->name, data) == 0;
}

// test_name_regex
struct test_name_regex {
	pcre *reg;
	pcre_extra *regext;
};

void *compile_regex(const char *pattern) {
	struct test_name_regex *regex = malloc(sizeof *regex);
	const char *error;
	int erroffset;
	if (!(regex->reg = pcre_compile(pattern, 0, &error, &erroffset, NULL))) {
		sway_log(L_ERROR, "Regex compilation failed:%s:%s", pattern, error);
		free(regex);
		return NULL;
	}
	regex->regext = pcre_study(regex->reg, 0, &error);
	if (error) {
		sway_log(L_DEBUG, "Regex study failed:%s:%s", pattern, error);
	}
	return regex;
}

void free_regex(void *_regex) {
	struct test_name_regex *regex = _regex;
	pcre_free(regex->reg);
	pcre_free_study(regex->regext);
	free(regex);
}

static bool exec_regex(const char *pattern, struct test_name_regex *regex) {
	int ovector[300];
	return 0 < pcre_exec(regex->reg, regex->regext, pattern,
			strlen(pattern), 0, 0, ovector, 300);
}

bool test_name_regex(swayc_t *view, void *data) {
	return view->name && exec_regex(view->name, data);
}
bool test_layout(swayc_t *view, void *data) {
	return view->layout & *(enum swayc_layouts *)data;
}
bool test_type(swayc_t *view, void *data) {
	return view->layout & *(enum swayc_types *)data;
}
bool test_visibility(swayc_t *view, void *data) {
	return view->visible == *(bool *)data;
}
bool test_handle(swayc_t *view, void *data) {
	return view->handle == *(wlc_handle *)data;
}

// C_VIEW tests
bool test_view_state(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& wlc_view_get_state(view->handle) & *(int *)data;
}
bool test_view_type(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& wlc_view_get_type(view->handle) & *(int *)data;
}
bool test_view_title(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& strcmp(view->name, data) == 0;
}
bool test_view_class(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& strcmp(wlc_view_get_class(view->handle), data) == 0;
}
bool test_view_appid(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& strcmp(wlc_view_get_app_id(view->handle), data) == 0;
}
bool test_view_title_regex(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& exec_regex(view->name, data);
}
bool test_view_class_regex(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& exec_regex(wlc_view_get_class(view->handle), data);
}
bool test_view_appid_regex(swayc_t *view, void *data) {
	return view->type == C_VIEW
		&& exec_regex(wlc_view_get_app_id(view->handle), data);
}

// Fancy test combiners
bool test_and(swayc_t *view, void *data) {
	struct test_list *list = data;
	while (list->test) {
		if (!list->test(view, list->data)) {
			return false;
		}
		++list;
	}
	return true;
}
bool test_or(swayc_t *view, void *data) {
	struct test_list *list = data;
	while (list->test) {
		if (list->test(view, list->data)) {
			return true;
		}
		++list;
	}
	return false;
}

// Focus|parent lookup

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

// Container information

bool swayc_is_fullscreen(swayc_t *view) {
	return view && view->type == C_VIEW
		&& wlc_view_get_state(view->handle) & WLC_BIT_FULLSCREEN;
}

bool swayc_is_active(swayc_t *view) {
	return view && view->type == C_VIEW
		&& wlc_view_get_state(view->handle) & WLC_BIT_ACTIVATED;
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

