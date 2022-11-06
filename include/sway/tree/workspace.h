#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include <stdbool.h>
#include "sway/config.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"

struct sway_view;

struct sway_workspace_state {
	struct sway_container *fullscreen;
	double x, y;
	int width, height;
	enum sway_container_layout layout;
	struct sway_output *output;
	list_t *floating;
	list_t *tiling;

	struct sway_container *focused_inactive_child;
	bool focused;
};

struct sway_workspace {
	struct sway_node node;
	struct sway_container *fullscreen;

	char *name;
	char *representation;

	double x, y;
	int width, height;
	enum sway_container_layout layout;
	enum sway_container_layout prev_split_layout;

	struct side_gaps current_gaps;
	int gaps_inner;
	struct side_gaps gaps_outer;

	struct sway_output *output; // NULL if no outputs are connected
	list_t *floating;           // struct sway_container
	list_t *tiling;             // struct sway_container
	list_t *output_priority;
	bool urgent;

	struct sway_workspace_state current;
};

struct workspace_config *workspace_find_config(const char *ws_name);

struct sway_output *workspace_get_initial_output(const char *name);

struct sway_workspace *workspace_create(struct sway_output *output,
		const char *name);

void workspace_destroy(struct sway_workspace *workspace);

void workspace_begin_destroy(struct sway_workspace *workspace);

void workspace_consider_destroy(struct sway_workspace *ws);

char *workspace_next_name(const char *output_name);

struct sway_workspace *workspace_auto_back_and_forth(
		struct sway_workspace *workspace);

bool workspace_switch(struct sway_workspace *workspace);

struct sway_workspace *workspace_by_number(const char* name);

struct sway_workspace *workspace_by_name(const char*);

struct sway_workspace *workspace_output_next(struct sway_workspace *current);

struct sway_workspace *workspace_next(struct sway_workspace *current);

struct sway_workspace *workspace_output_prev(struct sway_workspace *current);

struct sway_workspace *workspace_prev(struct sway_workspace *current);

bool workspace_is_visible(struct sway_workspace *ws);

bool workspace_is_empty(struct sway_workspace *ws);

void workspace_output_raise_priority(struct sway_workspace *workspace,
		struct sway_output *old_output, struct sway_output *new_output);

void workspace_output_add_priority(struct sway_workspace *workspace,
		struct sway_output *output);

struct sway_output *workspace_output_get_highest_available(
		struct sway_workspace *ws, struct sway_output *exclude);

void workspace_detect_urgent(struct sway_workspace *workspace);

void workspace_for_each_container(struct sway_workspace *ws,
		void (*f)(struct sway_container *con, void *data), void *data);

struct sway_container *workspace_find_container(struct sway_workspace *ws,
		bool (*test)(struct sway_container *con, void *data), void *data);

/**
 * Wrap the workspace's tiling children in a new container.
 * The new container will be the only direct tiling child of the workspace.
 * The new container is returned.
 */
struct sway_container *workspace_wrap_children(struct sway_workspace *ws);

void workspace_unwrap_children(struct sway_workspace *ws,
		struct sway_container *wrap);

void workspace_detach(struct sway_workspace *workspace);

struct sway_container *workspace_add_tiling(struct sway_workspace *workspace,
		struct sway_container *con);

void workspace_add_floating(struct sway_workspace *workspace,
		struct sway_container *con);

/**
 * Adds a tiling container to the workspace without considering
 * the workspace_layout, so the con will not be split.
 */
void workspace_insert_tiling_direct(struct sway_workspace *workspace,
		struct sway_container *con, int index);

struct sway_container *workspace_insert_tiling(struct sway_workspace *workspace,
		struct sway_container *con, int index);

void workspace_remove_gaps(struct sway_workspace *ws);

void workspace_add_gaps(struct sway_workspace *ws);

struct sway_container *workspace_split(struct sway_workspace *workspace,
		enum sway_container_layout layout);

void workspace_update_representation(struct sway_workspace *ws);

void workspace_get_box(struct sway_workspace *workspace, struct wlr_box *box);

size_t workspace_num_tiling_views(struct sway_workspace *ws);

size_t workspace_num_sticky_containers(struct sway_workspace *ws);

/**
 * workspace_squash is container_flatten in the reverse
 * direction. Instead of eliminating redundant splits that are
 * parents of the target container, it eliminates pairs of
 * redundant H/V splits that are children of the workspace.
 */
void workspace_squash(struct sway_workspace *workspace);

#endif
