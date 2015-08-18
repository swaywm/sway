#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H
#include <wlc/wlc.h>
typedef struct sway_container swayc_t;

#include "layout.h"

enum swayc_types{
	C_ROOT,
	C_OUTPUT,
	C_WORKSPACE,
	C_CONTAINER,
	C_VIEW,
	//Keep last
	C_TYPES,
};

enum swayc_layouts{
	L_NONE,
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING,
	//Keep last
	L_LAYOUTS,
};

struct sway_container {
	wlc_handle handle;

	enum swayc_types type;
	enum swayc_layouts layout;

	// Not including borders or margins
	int width, height;

	// Used for setting floating geometry
	int desired_width, desired_height;

	int x, y;

	bool visible;
	bool is_floating;
	bool is_focused;

	int weight;

	char *name;

	int gaps;

	list_t *children;
	list_t *floating;

	struct sway_container *parent;
	struct sway_container *focused;
};


swayc_t *new_output(wlc_handle handle);
swayc_t *new_workspace(swayc_t *output, const char *name);
// Creates container Around child (parent child) -> (parent (container child))
swayc_t *new_container(swayc_t *child, enum swayc_layouts layout);
// Creates view as a sibling of current focused container, or as child of a workspace
swayc_t *new_view(swayc_t *sibling, wlc_handle handle);
// Creates view as a new floating view which is in the active workspace
swayc_t *new_floating_view(wlc_handle handle);


swayc_t *destroy_output(swayc_t *output);
// Destroys workspace if empty and returns parent pointer, else returns NULL
swayc_t *destroy_workspace(swayc_t *workspace);
swayc_t *destroy_container(swayc_t *container);
swayc_t *destroy_view(swayc_t *view);

swayc_t *find_container(swayc_t *container, bool (*test)(swayc_t *view, void *data), void *data);
void container_map(swayc_t *, void (*f)(swayc_t *, void *), void *);

// Mappings
void set_view_visibility(swayc_t *view, void *data);

#endif
