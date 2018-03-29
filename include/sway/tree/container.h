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
struct sway_seat;

/**
 * Different kinds of containers.
 *
 * This enum is in order. A container will never be inside of a container below
 * it on this list.
 */
enum swayc_types {
	C_ROOT,
	C_OUTPUT,
	C_WORKSPACE,
	C_CONTAINER,
	C_VIEW,

	C_TYPES,
};

enum swayc_layouts {
	L_NONE,
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING,

	// Keep last
	L_LAYOUTS,
};

enum swayc_border_types {
	B_NONE,
	B_PIXEL,
	B_NORMAL,
};

struct sway_root;
struct sway_output;
struct sway_view;

struct sway_container {
	union {
		// TODO: Encapsulate state for other node types as well like C_CONTAINER
		struct sway_root *sway_root;
		struct sway_output *sway_output;
		struct sway_view *sway_view;
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

    // TODO convert to layout coordinates
	double x, y;

    // does not include borders or gaps.
	double width, height;

	list_t *children;

	struct sway_container *parent;

	list_t *marks; // list of char*

	struct {
		struct wl_signal destroy;
	} events;
};

void swayc_descendants_of_type(swayc_t *root, enum swayc_types type,
		void (*func)(swayc_t *item, void *data), void *data);

// TODO only one container create function and pass the type?
swayc_t *new_output(struct sway_output *sway_output);

swayc_t *new_workspace(swayc_t *output, const char *name);

swayc_t *new_view(swayc_t *sibling, struct sway_view *sway_view);

swayc_t *destroy_output(swayc_t *output);
swayc_t *destroy_view(swayc_t *view);

swayc_t *next_view_sibling(struct sway_seat *seat);

/**
 * Finds a container based on test criteria. Returns the first container that
 * passes the test.
 */
swayc_t *swayc_by_test(swayc_t *container,
		bool (*test)(swayc_t *view, void *data), void *data);

/**
 * Finds a parent container with the given swayc_type.
 */
swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types type);

/**
 * Maps a container's children over a function.
 */
void container_map(swayc_t *container,
		void (*f)(swayc_t *view, void *data), void *data);

swayc_t *swayc_at(swayc_t *parent, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

/**
 * Apply the function for each child of the container breadth first.
 */
void container_for_each_bfs(swayc_t *con, void (*f)(swayc_t *con, void *data),
		void *data);

swayc_t *swayc_change_layout(swayc_t *container, enum swayc_layouts layout);

#endif
