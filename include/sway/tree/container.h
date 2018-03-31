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
enum sway_container_type {
	C_ROOT,
	C_OUTPUT,
	C_WORKSPACE,
	C_CONTAINER,
	C_VIEW,

	C_TYPES,
};

enum sway_container_layout {
	L_NONE,
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING,

	// Keep last
	L_LAYOUTS,
};

enum sway_container_border {
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

	enum sway_container_type type;
	enum sway_container_layout layout;
	enum sway_container_layout prev_layout;
	enum sway_container_layout workspace_layout;

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

struct sway_container *container_create(enum sway_container_type type);

// TODO only one container create function and pass the type?
struct sway_container *container_output_create(
		struct sway_output *sway_output);

struct sway_container *container_workspace_create(
		struct sway_container *output, const char *name);

struct sway_container *container_view_create(
		struct sway_container *sibling, struct sway_view *sway_view);

struct sway_container *container_output_destroy(struct sway_container *output);

struct sway_container *container_workspace_destroy(
		struct sway_container *workspace);

struct sway_container *container_view_destroy(struct sway_container *view);

struct sway_container *container_destroy(struct sway_container *cont);

struct sway_container *container_set_layout(struct sway_container *container,
		enum sway_container_layout layout);

void container_descendants(struct sway_container *root,
		enum sway_container_type type,
		void (*func)(struct sway_container *item, void *data), void *data);

/**
 * Finds a container based on test criteria. Returns the first container that
 * passes the test.
 */
struct sway_container *container_find(struct sway_container *container,
		bool (*test)(struct sway_container *view, void *data), void *data);

/**
 * Finds a parent container with the given struct sway_containerype.
 */
struct sway_container *container_parent(struct sway_container *container,
		enum sway_container_type type);

/**
 * Find a container at the given coordinates.
 */
struct sway_container *container_at(struct sway_container *parent,
		double lx, double ly, struct wlr_surface **surface,
		double *sx, double *sy);

/**
 * Apply the function for each child of the container breadth first.
 */
void container_for_each_descendant_bfs(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data), void *data);

/**
 * Apply the function for each child of the container depth first.
 */
void container_for_each_descendant_dfs(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data), void *data);

bool container_has_anscestor(struct sway_container *descendant,
		struct sway_container *anscestor);

#endif
