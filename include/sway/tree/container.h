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

#define TITLEBAR_BORDER_THICKNESS 1

// Padding includes titlebar border
#define TITLEBAR_H_PADDING 3
#define TITLEBAR_V_PADDING 4

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

	// Keep last
	C_TYPES,
};

enum sway_container_layout {
	L_NONE,
	L_HORIZ,
	L_VERT,
	L_STACKED,
	L_TABBED,
	L_FLOATING,
};

enum sway_container_border {
	B_NONE,
	B_PIXEL,
	B_NORMAL,
};

struct sway_root;
struct sway_output;
struct sway_workspace;
struct sway_view;

struct sway_container {
	union {
		// TODO: Encapsulate state for other node types as well like C_CONTAINER
		struct sway_root *sway_root;
		struct sway_output *sway_output;
		struct sway_workspace *sway_workspace;
		struct sway_view *sway_view;
	};

	/**
	 * A unique ID to identify this container. Primarily used in the
	 * get_tree JSON output.
	 */
	size_t id;

	char *name;            // The view's title (unformatted)
	char *formatted_title; // The title displayed in the title bar

	enum sway_container_type type;
	enum sway_container_layout layout;
	enum sway_container_layout prev_layout;

	bool is_sticky;

	// For C_ROOT, this has no meaning
	// For other types, this is the position in layout coordinates
	// Includes borders
	double x, y;
	double width, height;
	double saved_x, saved_y;
	double saved_width, saved_height;

	list_t *children;

	struct sway_container *parent;

	list_t *marks; // list of char*

	float alpha;

	struct wlr_texture *title_focused;
	struct wlr_texture *title_focused_inactive;
	struct wlr_texture *title_unfocused;
	struct wlr_texture *title_urgent;
	size_t title_height;

	struct {
		struct wl_signal destroy;
		// Raised after the tree updates, but before arrange_windows
		// Passed the previous parent
		struct wl_signal reparent;
	} events;

	struct wl_listener reparent;
};

struct sway_container *container_create(enum sway_container_type type);

const char *container_type_to_str(enum sway_container_type type);

struct sway_container *output_create(struct sway_output *sway_output);

/**
 * Create a new container container. A container container can be a a child of
 * a workspace container or another container container.
 */
struct sway_container *container_container_create();

/**
 * Create a new output. Outputs are children of the root container and have no
 * order in the tree structure.
 */
struct sway_container *output_create(struct sway_output *sway_output);

/**
 * Create a new workspace container. Workspaces are children of an output
 * container and are ordered alphabetically by name.
 */
struct sway_container *workspace_create(struct sway_container *output,
		const char *name);

/*
 * Create a new view container. A view can be a child of a workspace container
 * or a container container and are rendered in the order and structure of
 * how they are attached to the tree.
 */
struct sway_container *container_view_create(
		struct sway_container *sibling, struct sway_view *sway_view);

struct sway_container *container_destroy(struct sway_container *container);

struct sway_container *container_close(struct sway_container *container);

void container_descendants(struct sway_container *root,
		enum sway_container_type type,
		void (*func)(struct sway_container *item, void *data), void *data);

/**
 * Search a container's descendants a container based on test criteria. Returns
 * the first container that passes the test.
 */
struct sway_container *container_find(struct sway_container *container,
		bool (*test)(struct sway_container *view, void *data), void *data);

/**
 * Finds a parent container with the given struct sway_containerype.
 */
struct sway_container *container_parent(struct sway_container *container,
		enum sway_container_type type);

/**
 * Find a container at the given coordinates. Returns the the surface and
 * surface-local coordinates of the given layout coordinates if the container
 * is a view and the view contains a surface at those coordinates.
 */
struct sway_container *container_at(struct sway_container *container,
		double ox, double oy, struct wlr_surface **surface,
		double *sx, double *sy);

/**
 * Same as container_at, but only checks floating views and expects coordinates
 * to be layout coordinates, as that's what floating views use.
 */
struct sway_container *floating_container_at(double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

/**
 * Apply the function for each descendant of the container breadth first.
 */
void container_for_each_descendant_bfs(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data), void *data);

/**
 * Apply the function for each child of the container depth first.
 */
void container_for_each_descendant_dfs(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data), void *data);

/**
 * Returns true if the given container is an ancestor of this container.
 */
bool container_has_ancestor(struct sway_container *container,
		struct sway_container *ancestor);

/**
 * Returns true if the given container is a child descendant of this container.
 */
bool container_has_child(struct sway_container *con,
		struct sway_container *child);

int container_count_descendants_of_type(struct sway_container *con,
		enum sway_container_type type);

void container_create_notify(struct sway_container *container);

void container_damage_whole(struct sway_container *container);

bool container_reap_empty(struct sway_container *con);

struct sway_container *container_reap_empty_recursive(
		struct sway_container *con);

struct sway_container *container_flatten(struct sway_container *container);

void container_update_title_textures(struct sway_container *container);

/**
 * Calculate the container's title_height property.
 */
void container_calculate_title_height(struct sway_container *container);

/**
 * Notify a container that a tree modification has changed in its children,
 * so the container can update its tree representation.
 */
void container_notify_subtree_changed(struct sway_container *container);

/**
 * Return the height of a regular title bar.
 */
size_t container_titlebar_height(void);

void container_set_floating(struct sway_container *container, bool enable);

void container_set_geometry_from_floating_view(struct sway_container *con);

/**
 * Determine if the given container is itself floating.
 * This will return false for any descendants of a floating container.
 */
bool container_is_floating(struct sway_container *container);

#endif
