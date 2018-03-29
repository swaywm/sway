#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H
#include <stdint.h>
#include <sys/types.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_surface.h>
#include "list.h"

extern struct sway_container root_container;

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

void swayc_descendants_of_type(struct sway_container *root,
		enum swayc_types type,
		void (*func)(struct sway_container *item, void *data), void *data);

// TODO only one container create function and pass the type?
struct sway_container *new_output(struct sway_output *sway_output);

struct sway_container *new_workspace(struct sway_container *output,
		const char *name);

struct sway_container *new_view(struct sway_container *sibling,
		struct sway_view *sway_view);

struct sway_container *destroy_output(struct sway_container *output);
struct sway_container *destroy_view(struct sway_container *view);

struct sway_container *next_view_sibling(struct sway_seat *seat);

/**
 * Finds a container based on test criteria. Returns the first container that
 * passes the test.
 */
struct sway_container *swayc_by_test(struct sway_container *container,
		bool (*test)(struct sway_container *view, void *data), void *data);

/**
 * Finds a parent container with the given swayc_type.
 */
struct sway_container *swayc_parent_by_type(struct sway_container *container,
		enum swayc_types type);

/**
 * Maps a container's children over a function.
 */
void container_map(struct sway_container *container,
		void (*f)(struct sway_container *view, void *data), void *data);

struct sway_container *swayc_at(struct sway_container *parent, double lx,
		double ly, struct wlr_surface **surface, double *sx, double *sy);

/**
 * Apply the function for each child of the container breadth first.
 */
void container_for_each_bfs(struct sway_container *con, void (*f)(struct
			sway_container *con, void *data), void *data);

struct sway_container *swayc_change_layout(struct sway_container *container,
		enum swayc_layouts layout);

#endif
