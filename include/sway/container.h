#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H
#include <stdint.h>
#include <sys/types.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_surface.h>
#include "list.h"

typedef struct sway_container swayc_t;

extern swayc_t root_container;

struct sway_view;

/**
 * Different kinds of containers.
 *
 * This enum is in order. A container will never be inside of a container below
 * it on this list.
 */
enum swayc_types {
	C_ROOT,      /**< The root container. Only one of these ever exists. */
	C_OUTPUT,    /**< An output (aka monitor, head, etc). */
	C_WORKSPACE, /**< A workspace. */
	C_CONTAINER, /**< A manually created container. */
	C_VIEW,      /**< A view (aka window). */

	C_TYPES,
};

/**
 * Different ways to arrange a container.
 */
enum swayc_layouts {
	L_NONE,     /**< Used for containers that have no layout (views, root) */
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING, /**< A psuedo-container, removed from the tree, to hold floating windows */

	/* Awesome/Monad style auto layouts */
	L_AUTO_LEFT,
	L_AUTO_RIGHT,
	L_AUTO_TOP,
	L_AUTO_BOTTOM,

	L_AUTO_FIRST = L_AUTO_LEFT,
	L_AUTO_LAST = L_AUTO_BOTTOM,

	// Keep last
	L_LAYOUTS,
};

enum swayc_border_types {
	B_NONE,   /**< No border */
	B_PIXEL,  /**< 1px border */
	B_NORMAL, /**< Normal border with title bar */
};

struct sway_output;
struct sway_view;
struct wlr_output_layout;

/**
 * Stores information about a container.
 *
 * The tree is made of these. Views are containers that cannot have children.
 */
struct sway_container {
	union {
		// TODO: Encapsulate state for other node types as well like C_CONTAINER
		struct wlr_output_layout *output_layout; // C_ROOT
		struct sway_output *sway_output;         // C_OUTPUT
		struct sway_view *sway_view;             // C_VIEW
	};

	/**
	 * A unique ID to identify this container. Primarily used in the
	 * get_tree JSON output.
	 */
	size_t id;

	char *name;

	enum swayc_types type;
	enum swayc_layouts layout;
	enum swayc_layouts prev_layout;
	enum swayc_layouts workspace_layout;

	/**
	 * The coordinates that this view appear at, relative to the output they
	 * are located on (output containers have absolute coordinates).
	 */
	double x, y;

	/**
	 * Width and height of this container, without borders or gaps.
	 */
	double width, height;

	list_t *children;

	/**
	 * The parent of this container. NULL for the root container.
	 */
	struct sway_container *parent;
	/**
	 * Which of this container's children has focus.
	 */
	struct sway_container *focused;

	/**
	 * Number of master views in auto layouts.
	 */
	size_t nb_master;

	/**
	 * Number of slave groups (e.g. columns) in auto layouts.
	 */
	size_t nb_slave_groups;

	/**
	 * Marks applied to the container, list_t of char*.
	 */
	list_t *marks;

	struct {
		struct wl_signal destroy;
	} events;
};

void swayc_descendants_of_type(swayc_t *root, enum swayc_types type,
		void (*func)(swayc_t *item, void *data), void *data);

swayc_t *new_output(struct sway_output *sway_output);
swayc_t *new_workspace(swayc_t *output, const char *name);
swayc_t *new_view(swayc_t *sibling, struct sway_view *sway_view);

swayc_t *destroy_view(swayc_t *view);

swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types type);

swayc_t *swayc_at(swayc_t *parent, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

#endif
