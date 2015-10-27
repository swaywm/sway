#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H
#include <wlc/wlc.h>
typedef struct sway_container swayc_t;

#include "layout.h"

extern struct wlc_origin mouse_origin;

enum swayc_types{
	C_ROOT,
	C_OUTPUT,
	C_WORKSPACE,
	C_CONTAINER,
	C_VIEW,
	// Keep last
	C_TYPES,
};


enum swayc_layouts{
	L_NONE,
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING,
	// Keep last
	L_LAYOUTS,
};

struct sway_container {
	wlc_handle handle;

	enum swayc_types type;
	enum swayc_layouts layout;

	// Not including borders or margins
	double width, height;

	// Used for setting floating geometry
	int desired_width, desired_height;

	double x, y;

	bool visible;
	bool is_floating;
	bool is_focused;

	char *name;

	int gaps;

	list_t *children;
	list_t *floating;

	struct sway_container *parent;
	struct sway_container *focused;
};

enum visibility_mask {
	VISIBLE = true
} visible;

// Container Creation

swayc_t *new_output(wlc_handle handle);
swayc_t *new_workspace(swayc_t *output, const char *name);
// Creates container Around child (parent child) -> (parent (container child))
swayc_t *new_container(swayc_t *child, enum swayc_layouts layout);
// Creates view as a sibling of current focused container, or as child of a workspace
swayc_t *new_view(swayc_t *sibling, wlc_handle handle);
// Creates view as a new floating view which is in the active workspace
swayc_t *new_floating_view(wlc_handle handle);

// Container Destroying

swayc_t *destroy_output(swayc_t *output);
// Destroys workspace if empty and returns parent pointer, else returns NULL
swayc_t *destroy_workspace(swayc_t *workspace);
// Destroyes container and all parent container if they are empty, returns
// topmost non-empty parent. returns NULL otherwise
swayc_t *destroy_container(swayc_t *container);
// Destroys view and all empty parent containers. return topmost non-empty
// parent
swayc_t *destroy_view(swayc_t *view);

// Container Lookup

swayc_t *swayc_by_test(swayc_t *container, bool (*test)(swayc_t *view, void *data), void *data);
swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types);
swayc_t *swayc_parent_by_layout(swayc_t *container, enum swayc_layouts);
// Follow focused until type/layout
swayc_t *swayc_focus_by_type(swayc_t *container, enum swayc_types);
swayc_t *swayc_focus_by_layout(swayc_t *container, enum swayc_layouts);


swayc_t *swayc_by_handle(wlc_handle handle);
swayc_t *swayc_by_name(const char *name);
swayc_t *swayc_active_output(void);
swayc_t *swayc_active_workspace(void);
swayc_t *swayc_active_workspace_for(swayc_t *view);
// set focus to current pointer location and return focused container
swayc_t *container_under_pointer(void);

// Container information

bool swayc_is_fullscreen(swayc_t *view);
bool swayc_is_active(swayc_t *view);
// Is `parent` the parent of `child`
bool swayc_is_parent_of(swayc_t *parent, swayc_t *child);
// Is `child` a child of `parent`
bool swayc_is_child_of(swayc_t *child, swayc_t *parent);
// Return gap of specified container
int swayc_gap(swayc_t *container);

// Mapping functions

void container_map(swayc_t *, void (*f)(swayc_t *, void *), void *);

// Mappings
void set_view_visibility(swayc_t *view, void *data);
// Set or add to gaps
void set_gaps(swayc_t *view, void *amount);
void add_gaps(swayc_t *view, void *amount);

void update_visibility(swayc_t *container);

#endif
