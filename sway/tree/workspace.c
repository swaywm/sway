#define _XOPEN_SOURCE 500
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "sway/tree/container.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "util.h"

char *prev_workspace_name = NULL;
struct workspace_by_number_data {
	int len;
	const char *cset;
	const char *name;
};

void next_name_map(swayc_t *ws, void *data) {
	int *count = data;
	++count;
}

char *workspace_next_name(const char *output_name) {
	wlr_log(L_DEBUG, "Workspace: Generating new workspace name for output %s",
			output_name);
	int count = 0;
	next_name_map(&root_container, &count);
	++count;
	int len = snprintf(NULL, 0, "%d", count);
	char *name = malloc(len + 1);
	if (!sway_assert(name, "Failed to allocate workspace name")) {
		return NULL;
	}
	snprintf(name, len + 1, "%d", count);
	return name;
}

static bool _workspace_by_number(swayc_t *view, void *data) {
	if (view->type != C_WORKSPACE) {
		return false;
	}
	struct workspace_by_number_data *wbnd = data;
	int a = strspn(view->name, wbnd->cset);
	return a == wbnd->len && strncmp(view->name, wbnd->name, a) == 0;
}

swayc_t *workspace_by_number(const char* name) {
	struct workspace_by_number_data wbnd = {0, "1234567890", name};
	wbnd.len = strspn(name, wbnd.cset);
	if (wbnd.len <= 0) {
		return NULL;
	}
	return swayc_by_test(&root_container, _workspace_by_number, (void *) &wbnd);
}

static bool _workspace_by_name(swayc_t *view, void *data) {
	return (view->type == C_WORKSPACE) &&
		   (strcasecmp(view->name, (char *) data) == 0);
}

swayc_t *workspace_by_name(const char *name) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	swayc_t *current_workspace = NULL, *current_output = NULL;
	swayc_t *focus = sway_seat_get_focus(seat);
	if (focus) {
		current_workspace = swayc_parent_by_type(focus, C_WORKSPACE);
		current_output = swayc_parent_by_type(focus, C_OUTPUT);
	}
	if (strcmp(name, "prev") == 0) {
		return workspace_prev(current_workspace);
	} else if (strcmp(name, "prev_on_output") == 0) {
		return workspace_output_prev(current_output);
	} else if (strcmp(name, "next") == 0) {
		return workspace_next(current_workspace);
	} else if (strcmp(name, "next_on_output") == 0) {
		return workspace_output_next(current_output);
	} else if (strcmp(name, "current") == 0) {
		return current_workspace;
	} else {
		return swayc_by_test(&root_container, _workspace_by_name, (void *) name);
	}
}

swayc_t *workspace_create(const char *name) {
	swayc_t *parent;
	// Search for workspace<->output pair
	int i, e = config->workspace_outputs->length;
	for (i = 0; i < e; ++i) {
		struct workspace_output *wso = config->workspace_outputs->items[i];
		if (strcasecmp(wso->workspace, name) == 0) {
			// Find output to use if it exists
			e = root_container.children->length;
			for (i = 0; i < e; ++i) {
				parent = root_container.children->items[i];
				if (strcmp(parent->name, wso->output) == 0) {
					return new_workspace(parent, name);
				}
			}
			break;
		}
	}
	// Otherwise create a new one
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	swayc_t *focus = sway_seat_get_focus_inactive(seat, &root_container);
	parent = focus;
	parent = swayc_parent_by_type(parent, C_OUTPUT);
	return new_workspace(parent, name);
}

/**
 * Get the previous or next workspace on the specified output. Wraps around at
 * the end and beginning.  If next is false, the previous workspace is returned,
 * otherwise the next one is returned.
 */
swayc_t *workspace_output_prev_next_impl(swayc_t *output, bool next) {
	if (!sway_assert(output->type == C_OUTPUT,
				"Argument must be an output, is %d", output->type)) {
		return NULL;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	swayc_t *focus = sway_seat_get_focus_inactive(seat, output);
	swayc_t *workspace = (focus->type == C_WORKSPACE ?
			focus :
			swayc_parent_by_type(focus, C_WORKSPACE));

	int i;
	for (i = 0; i < output->children->length; i++) {
		if (output->children->items[i] == workspace) {
			return output->children->items[
				wrap(i + (next ? 1 : -1), output->children->length)];
		}
	}

	// Doesn't happen, at worst the for loop returns the previously active workspace
	return NULL;
}

/**
 * Get the previous or next workspace. If the first/last workspace on an output
 * is active, proceed to the previous/next output's previous/next workspace. If
 * next is false, the previous workspace is returned, otherwise the next one is
 * returned.
 */
swayc_t *workspace_prev_next_impl(swayc_t *workspace, bool next) {
	if (!sway_assert(workspace->type == C_WORKSPACE,
				"Argument must be a workspace, is %d", workspace->type)) {
		return NULL;
	}

	swayc_t *current_output = workspace->parent;
	int offset = next ? 1 : -1;
	int start = next ? 0 : 1;
	int end;
	if (next) {
		end = current_output->children->length - 1;
	} else {
		end = current_output->children->length;
	}
	int i;
	for (i = start; i < end; i++) {
		if (current_output->children->items[i] == workspace) {
			return current_output->children->items[i + offset];
		}
	}

	// Given workspace is the first/last on the output, jump to the previous/next output
	int num_outputs = root_container.children->length;
	for (i = 0; i < num_outputs; i++) {
		if (root_container.children->items[i] == current_output) {
			swayc_t *next_output = root_container.children->items[
				wrap(i + offset, num_outputs)];
			return workspace_output_prev_next_impl(next_output, next);
		}
	}

	// Doesn't happen, at worst the for loop returns the previously active workspace on the active output
	return NULL;
}

swayc_t *workspace_output_next(swayc_t *current) {
	return workspace_output_prev_next_impl(current, true);
}

swayc_t *workspace_next(swayc_t *current) {
	return workspace_prev_next_impl(current, true);
}

swayc_t *workspace_output_prev(swayc_t *current) {
	return workspace_output_prev_next_impl(current, false);
}

swayc_t *workspace_prev(swayc_t *current) {
	return workspace_prev_next_impl(current, false);
}

bool workspace_switch(swayc_t *workspace) {
	if (!workspace) {
		return false;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	swayc_t *focus = sway_seat_get_focus_inactive(seat, &root_container);
	if (!seat || !focus) {
		return false;
	}
	swayc_t *active_ws = focus;
	if (active_ws->type != C_WORKSPACE) {
		swayc_parent_by_type(focus, C_WORKSPACE);
	}

	if (config->auto_back_and_forth
			&& active_ws == workspace
			&& prev_workspace_name) {
		swayc_t *new_ws = workspace_by_name(prev_workspace_name);
		workspace = new_ws ? new_ws : workspace_create(prev_workspace_name);
	}

	if (!prev_workspace_name || (strcmp(prev_workspace_name, active_ws->name)
				&& active_ws != workspace)) {
		free(prev_workspace_name);
		prev_workspace_name = malloc(strlen(active_ws->name) + 1);
		if (!prev_workspace_name) {
			wlr_log(L_ERROR, "Unable to allocate previous workspace name");
			return false;
		}
		strcpy(prev_workspace_name, active_ws->name);
	}

	// TODO: Deal with sticky containers

	wlr_log(L_DEBUG, "Switching to workspace %p:%s", workspace, workspace->name);
	swayc_t *next = sway_seat_get_focus_inactive(seat, workspace);
	if (next == NULL) {
		next = workspace;
	}
	sway_seat_set_focus(seat, next);
	swayc_t *output = swayc_parent_by_type(workspace, C_OUTPUT);
	arrange_windows(output, -1, -1);
	return true;
}
