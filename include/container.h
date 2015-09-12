#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H
#include <wlc/wlc.h>
typedef struct sway_container swayc_t;
#include "layout.h"

enum swayc_types {
	C_ROOT      = 1 << 0,
	C_OUTPUT    = 1 << 1,
	C_WORKSPACE = 1 << 2,
	C_CONTAINER = 1 << 3,
	C_VIEW      = 1 << 4,
	C_TYPES     = 5,
};

enum swayc_layouts {
	L_NONE     = 1 << 0,
	L_HORIZ    = 1 << 1,
	L_VERT     = 1 << 2,
	L_STACKED  = 1 << 3,
	L_TABBED   = 1 << 4,
	L_FLOATING = 1 << 5,
	L_LAYOUTS  = 6,
};

struct sway_container {
	wlc_handle handle;

	enum swayc_types type;
	enum swayc_layouts layout;

	// Not including borders or margins
	double width, height;
	double x, y;

	// Used for setting floating geometry
	int desired_width, desired_height;

	enum visibility_mask {
		INVISIBLE = 0,
		VISIBLE = 1,
	} visible;

	bool is_floating;
	bool is_focused;

	char *name;

	int gaps;

	list_t *children;
	list_t *floating;

	struct sway_container *parent;
	struct sway_container *focused;
};

// swayc Creation

/* Creates and returns new, or an already created output.
 * If it creates a new output, it also creates a workspace using
 * `new_workspace(outputname, NULL);` */
swayc_t *new_output(wlc_handle handle);

/* Creates workspace with given name, under given output.
 * If workspace with that name already exists, returns that workspace
 * If name is NULL, it will choose a name automatically.
 * If output is NULL, it will choose an output automatically. */
swayc_t *new_workspace(swayc_t *output, const char *name);

// Creates container Around child (parent child) -> (parent (container child))
swayc_t *new_container(swayc_t *child, enum swayc_layouts layout);

// Creates view as a sibling of current focused container, or as child of a workspace
swayc_t *new_view(swayc_t *sibling, wlc_handle handle);

// Creates view as a new floating view which is in the active workspace
swayc_t *new_floating_view(wlc_handle handle);

// Container Destroying
// Destroys output and moves workspaces to another output
swayc_t *destroy_output(swayc_t *output);

// Destroys workspace if empty and returns parent pointer, else returns NULL
swayc_t *destroy_workspace(swayc_t *workspace);

// Destroyes container and all parent container if they are empty, returns
// topmost non-empty parent. returns NULL otherwise
swayc_t *destroy_container(swayc_t *container);

// Destroys view and all empty parent containers. return topmost non-empty
// parent
swayc_t *destroy_view(swayc_t *view);

// Container Mapping and testing functions
typedef bool swayc_test_func(swayc_t *view, void *data);
typedef void swayc_map_func(swayc_t *view, void *data);

// Returns the first swayc that matches test()
swayc_t *swayc_by_test_r(swayc_t *root, swayc_test_func test, void *data);
swayc_t *swayc_by_test(swayc_test_func test, void *data);

// Calls func for all children.
void swayc_map_r(swayc_t *root, swayc_map_func func, void *data);
void swayc_map(swayc_map_func func, void *data);


// Call func on container if test passes
void swayc_map_by_test_r(swayc_t *root,
		swayc_map_func func, swayc_test_func test,
		void *funcdata, void *testdata);
void swayc_map_by_test(
		swayc_map_func func, swayc_test_func test,
		void *funcdata, void *testdata);

// Map functions
swayc_map_func set_gaps;
swayc_map_func add_gaps;

// Test functions
// generic swayc tests
swayc_test_func test_name;
swayc_test_func test_name_regex;
swayc_test_func test_layout;
swayc_test_func test_type;
swayc_test_func test_visibility;
swayc_test_func test_handle;

// C_VIEW tests
// See wlc_view_*_bit enums
swayc_test_func test_view_state;
swayc_test_func test_view_type;
swayc_test_func test_view_title;
swayc_test_func test_view_class;
swayc_test_func test_view_appid;
swayc_test_func test_view_title_regex;
swayc_test_func test_view_class_regex;
swayc_test_func test_view_appid_regex;

// functions for test_*_regex
void *compile_regex(const char *regex);
void free_regex(void *);

// these take a NULL terminated array of test_list struct.
struct test_list { swayc_test_func *test; void *data ; };
swayc_test_func test_and;
swayc_test_func test_or;

swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types);
swayc_t *swayc_parent_by_layout(swayc_t *container, enum swayc_layouts);
// Follow focused until type/layout
swayc_t *swayc_focus_by_type(swayc_t *container, enum swayc_types);
swayc_t *swayc_focus_by_layout(swayc_t *container, enum swayc_layouts);

swayc_t *swayc_active_output(void);
swayc_t *swayc_active_workspace(void);
swayc_t *swayc_active_workspace_for(swayc_t *view);

// Container information
// if `parent` is the parent of `child`
bool swayc_is_parent_of(swayc_t *parent, swayc_t *child);
// If `child` is a child of `parent`
bool swayc_is_child_of(swayc_t *child, swayc_t *parent);
// Return gap of specified container
int swayc_gap(swayc_t *container);

bool swayc_is_fullscreen(swayc_t *view);
bool swayc_is_active(swayc_t *view);


// Specialized mapping functions
void update_visibility(swayc_t *container);

#endif
