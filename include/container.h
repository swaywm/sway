#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H
#include <wlc/wlc.h>
typedef struct sway_container swayc_t;

#include "layout.h"

/**
 * Different kinds of containers.
 * 
 * This enum is in order. A container will never be inside of a container below
 * it on this list.
 */
enum swayc_types {
	C_ROOT,				/**< The root container. Only one of these ever exists. */
	C_OUTPUT,			/**< An output (aka monitor, head, etc). */
	C_WORKSPACE,		/**< A workspace. */
	C_CONTAINER,		/**< A manually created container. */
	C_VIEW,				/**< A view (aka window). */
	// Keep last
	C_TYPES,
};

/**
 * Different ways to arrange a container.
 */
enum swayc_layouts {
	L_NONE, 			/**< Used for containers that have no layout (views, root) */
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING,			/**< A psuedo-container, removed from the tree, to hold floating windows */
	// Keep last
	L_LAYOUTS,
};

/**
 * Stores information about a container.
 *
 * The tree is made of these. Views are containers that cannot have children.
 */
struct sway_container {
	/**
	 * If this container maps to a WLC object, this is set to that object's
	 * handle. Otherwise, NULL.
	 */
	wlc_handle handle;

	enum swayc_types type;
	enum swayc_layouts layout;

	/**
	 * Width and height of this container, without borders or gaps.
	 */
	double width, height;

	/**
	 * Views may request geometry, which is stored in this and ignored until
	 * the views are floated.
	 */
	int desired_width, desired_height;

	/**
	 * The coordinates that this view appear at, relative to the output they
	 * are located on (output containers have absolute coordinates).
	 */
	double x, y;

	/**
	 * False if this view is invisible. It could be in the scratchpad or on a
	 * workspace that is not shown.
	 */
	bool visible;
	bool is_floating;
	bool is_focused;
	bool sticky; // floating view always visible on its output

	// Attributes that mostly views have.
	char *name;
	char *class;
	char *app_id;

	int gaps;

	list_t *children;
	/**
	 * Children of this container that are floated.
	 */
	list_t *floating;

	/**
	 * The parent of this container. NULL for the root container.
	 */
	struct sway_container *parent;
	/**
	 * Which of this container's children has focus.
	 */
	struct sway_container *focused;
};

enum visibility_mask {
	VISIBLE = true
} visible;

/**
 * Allocates a new output container.
 */
swayc_t *new_output(wlc_handle handle);
/**
 * Allocates a new workspace container.
 */
swayc_t *new_workspace(swayc_t *output, const char *name);
/**
 * Allocates a new container and places a child into it.
 *
 * This is used from the split command, which creates a new container with the
 * requested layout and replaces the focused container in the tree with the new
 * one. Then the removed container is added as a child of the new container.
 */
swayc_t *new_container(swayc_t *child, enum swayc_layouts layout);
/**
 * Allocates a new view container.
 *
 * Pass in a sibling view, or a workspace to become this container's parent.
 */
swayc_t *new_view(swayc_t *sibling, wlc_handle handle);
/**
 * Allocates a new floating view in the active workspace.
 */
swayc_t *new_floating_view(wlc_handle handle);

/**
 * Frees an output's container.
 */
swayc_t *destroy_output(swayc_t *output);
/**
 * Destroys a workspace container and returns the parent pointer, or NULL.
 */
swayc_t *destroy_workspace(swayc_t *workspace);
/**
 * Destroys a container and all empty parents. Returns the topmost non-empty
 * parent container, or NULL.
 */
swayc_t *destroy_container(swayc_t *container);
/**
 * Destroys a view container and all empty parents. Returns the topmost
 * non-empty parent container, or NULL.
 */
swayc_t *destroy_view(swayc_t *view);

/**
 * Finds a container based on test criteria. Returns the first container that
 * passes the test.
 */
swayc_t *swayc_by_test(swayc_t *container, bool (*test)(swayc_t *view, void *data), void *data);
/**
 * Finds a parent container with the given swayc_type.
 */
swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types);
/**
 * Finds a parent with the given swayc_layout.
 */
swayc_t *swayc_parent_by_layout(swayc_t *container, enum swayc_layouts);
/**
 * Finds the bottom-most focused container of a type.
 */
swayc_t *swayc_focus_by_type(swayc_t *container, enum swayc_types);
/**
 * Finds the bottom-most focused container of a layout.
 */
swayc_t *swayc_focus_by_layout(swayc_t *container, enum swayc_layouts);

/**
 * Gets the swayc_t associated with a wlc_handle.
 */
swayc_t *swayc_by_handle(wlc_handle handle);
/**
 * Gets the named swayc_t.
 */
swayc_t *swayc_by_name(const char *name);
/**
 * Gets the active output's container.
 */
swayc_t *swayc_active_output(void);
/**
 * Gets the active workspace's container.
 */
swayc_t *swayc_active_workspace(void);
/**
 * Gets the workspace for the given view container.
 */
swayc_t *swayc_active_workspace_for(swayc_t *view);
/**
 * Finds the container currently underneath the pointer.
 */
swayc_t *container_under_pointer(void);

/**
 * Returns true if a container is fullscreen.
 */
bool swayc_is_fullscreen(swayc_t *view);
/**
 * Returns true if this view is focused.
 */
bool swayc_is_active(swayc_t *view);
/**
 * Returns true if the parent is an ancestor of the child.
 */
bool swayc_is_parent_of(swayc_t *parent, swayc_t *child);
/**
 * Returns true if the child is a desecendant of the parent.
 */
bool swayc_is_child_of(swayc_t *child, swayc_t *parent);
/**
 * Returns the gap (padding) of the container.
 *
 * This returns the inner gaps for a view, the outer gaps for a workspace, and
 * 0 otherwise.
 */
int swayc_gap(swayc_t *container);

/**
 * Maps a container's children over a function.
 */
void container_map(swayc_t *, void (*f)(swayc_t *, void *), void *);

/**
 * Set a view as visible or invisible.
 *
 * This will perform the required wlc calls as well; it is not sufficient to
 * simply toggle the boolean in swayc_t.
 */
void set_view_visibility(swayc_t *view, void *data);
/**
 * Set the gaps value for a view.
 */
void set_gaps(swayc_t *view, void *amount);
/**
 * Add to the gaps value for a view.
 */
void add_gaps(swayc_t *view, void *amount);

/**
 * Issue wlc calls to make the visibility of a container consistent.
 */
void update_visibility(swayc_t *container);

#endif
