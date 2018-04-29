#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "stringop.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "util.h"

static struct sway_container *get_workspace_initial_output(const char *name) {
	struct sway_container *parent;
	// Search for workspace<->output pair
	int e = config->workspace_outputs->length;
	for (int i = 0; i < config->workspace_outputs->length; ++i) {
		struct workspace_output *wso = config->workspace_outputs->items[i];
		if (strcasecmp(wso->workspace, name) == 0) {
			// Find output to use if it exists
			e = root_container.children->length;
			for (i = 0; i < e; ++i) {
				parent = root_container.children->items[i];
				if (strcmp(parent->name, wso->output) == 0) {
					return parent;
				}
			}
			break;
		}
	}
	// Otherwise put it on the focused output
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, &root_container);
	parent = focus;
	parent = container_parent(parent, C_OUTPUT);
	return parent;
}

struct sway_container *workspace_create(struct sway_container *output,
		const char *name) {
	if (output == NULL) {
		output = get_workspace_initial_output(name);
	}

	wlr_log(L_DEBUG, "Added workspace %s for output %s", name, output->name);
	struct sway_container *workspace = container_create(C_WORKSPACE);

	workspace->x = output->x;
	workspace->y = output->y;
	workspace->width = output->width;
	workspace->height = output->height;
	workspace->name = !name ? NULL : strdup(name);
	workspace->prev_layout = L_NONE;
	workspace->layout = container_get_default_layout(output);

	struct sway_workspace *swayws = calloc(1, sizeof(struct sway_workspace));
	if (!swayws) {
		return NULL;
	}
	swayws->swayc = workspace;
	workspace->sway_workspace = swayws;

	container_add_child(output, workspace);
	container_sort_workspaces(output);
	container_create_notify(workspace);

	return workspace;
}

char *prev_workspace_name = NULL;
struct workspace_by_number_data {
	int len;
	const char *cset;
	const char *name;
};

void next_name_map(struct sway_container *ws, void *data) {
	int *count = data;
	++count;
}

static bool workspace_valid_on_output(const char *output_name,
		const char *ws_name) {
	int i;
	for (i = 0; i < config->workspace_outputs->length; ++i) {
		struct workspace_output *wso = config->workspace_outputs->items[i];
		if (strcasecmp(wso->workspace, ws_name) == 0) {
			if (strcasecmp(wso->output, output_name) != 0) {
				return false;
			}
		}
	}

	return true;
}

char *workspace_next_name(const char *output_name) {
	wlr_log(L_DEBUG, "Workspace: Generating new workspace name for output %s",
			output_name);
	int l = 1;
	// Scan all workspace bindings to find the next available workspace name,
	// if none are found/available then default to a number
	struct sway_mode *mode = config->current_mode;

	// TODO: iterate over keycode bindings too
	int order = INT_MAX;
	char *target = NULL;
	for (int i = 0; i < mode->keysym_bindings->length; ++i) {
		struct sway_binding *binding = mode->keysym_bindings->items[i];
		char *cmdlist = strdup(binding->command);
		char *dup = cmdlist;
		char *name = NULL;

		// workspace n
		char *cmd = argsep(&cmdlist, " ");
		if (cmdlist) {
			name = argsep(&cmdlist, ",;");
		}

		if (strcmp("workspace", cmd) == 0 && name) {
			wlr_log(L_DEBUG, "Got valid workspace command for target: '%s'", name);
			char *_target = strdup(name);
			strip_quotes(_target);
			while (isspace(*_target)) {
				memmove(_target, _target+1, strlen(_target+1));
			}

			// Make sure that the command references an actual workspace
			// not a command about workspaces
			if (strcmp(_target, "next") == 0 ||
				strcmp(_target, "prev") == 0 ||
				strcmp(_target, "next_on_output") == 0 ||
				strcmp(_target, "prev_on_output") == 0 ||
				strcmp(_target, "number") == 0 ||
				strcmp(_target, "back_and_forth") == 0 ||
				strcmp(_target, "current") == 0)
			{
				free(_target);
				free(dup);
				continue;
			}

			// If the command is workspace number <name>, isolate the name
			if (strncmp(_target, "number ", strlen("number ")) == 0) {
				size_t length = strlen(_target) - strlen("number ") + 1;
				char *temp = malloc(length);
				strncpy(temp, _target + strlen("number "), length - 1);
				temp[length - 1] = '\0';
				free(_target);
				_target = temp;
				wlr_log(L_DEBUG, "Isolated name from workspace number: '%s'", _target);

				// Make sure the workspace number doesn't already exist
				if (workspace_by_number(_target)) {
					free(_target);
					free(dup);
					continue;
				}
			}

			// Make sure that the workspace doesn't already exist
			if (workspace_by_name(_target)) {
				free(_target);
				free(dup);
				continue;
			}

			// make sure that the workspace can appear on the given
			// output
			if (!workspace_valid_on_output(output_name, _target)) {
				free(_target);
				free(dup);
				continue;
			}

			if (binding->order < order) {
				order = binding->order;
				free(target);
				target = _target;
				wlr_log(L_DEBUG, "Workspace: Found free name %s", _target);
			}
		}
		free(dup);
	}
	if (target != NULL) {
		return target;
	}
	// As a fall back, get the current number of active workspaces
	// and return that + 1 for the next workspace's name
	int ws_num = root_container.children->length;
	if (ws_num >= 10) {
		l = 2;
	} else if (ws_num >= 100) {
		l = 3;
	}
	char *name = malloc(l + 1);
	if (!name) {
		wlr_log(L_ERROR, "Could not allocate workspace name");
		return NULL;
	}
	sprintf(name, "%d", ws_num++);
	return name;
}

static bool _workspace_by_number(struct sway_container *view, void *data) {
	if (view->type != C_WORKSPACE) {
		return false;
	}
	struct workspace_by_number_data *wbnd = data;
	int a = strspn(view->name, wbnd->cset);
	return a == wbnd->len && strncmp(view->name, wbnd->name, a) == 0;
}

struct sway_container *workspace_by_number(const char* name) {
	struct workspace_by_number_data wbnd = {0, "1234567890", name};
	wbnd.len = strspn(name, wbnd.cset);
	if (wbnd.len <= 0) {
		return NULL;
	}
	return container_find(&root_container,
			_workspace_by_number, (void *) &wbnd);
}

static bool _workspace_by_name(struct sway_container *view, void *data) {
	return (view->type == C_WORKSPACE) &&
		   (strcasecmp(view->name, (char *) data) == 0);
}

struct sway_container *workspace_by_name(const char *name) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *current_workspace = NULL, *current_output = NULL;
	struct sway_container *focus = seat_get_focus(seat);
	if (focus) {
		current_workspace = container_parent(focus, C_WORKSPACE);
		current_output = container_parent(focus, C_OUTPUT);
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
		return container_find(&root_container, _workspace_by_name,
				(void *)name);
	}
}

/**
 * Get the previous or next workspace on the specified output. Wraps around at
 * the end and beginning.  If next is false, the previous workspace is returned,
 * otherwise the next one is returned.
 */
struct sway_container *workspace_output_prev_next_impl(
		struct sway_container *output, bool next) {
	if (!sway_assert(output->type == C_OUTPUT,
				"Argument must be an output, is %d", output->type)) {
		return NULL;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus_inactive(seat, output);
	struct sway_container *workspace = (focus->type == C_WORKSPACE ?
			focus :
			container_parent(focus, C_WORKSPACE));

	int i;
	for (i = 0; i < output->children->length; i++) {
		if (output->children->items[i] == workspace) {
			return output->children->items[
				wrap(i + (next ? 1 : -1), output->children->length)];
		}
	}

	// Doesn't happen, at worst the for loop returns the previously active
	// workspace
	return NULL;
}

/**
 * Get the previous or next workspace. If the first/last workspace on an output
 * is active, proceed to the previous/next output's previous/next workspace. If
 * next is false, the previous workspace is returned, otherwise the next one is
 * returned.
 */
struct sway_container *workspace_prev_next_impl(
		struct sway_container *workspace, bool next) {
	if (!sway_assert(workspace->type == C_WORKSPACE,
				"Argument must be a workspace, is %d", workspace->type)) {
		return NULL;
	}

	struct sway_container *current_output = workspace->parent;
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

	// Given workspace is the first/last on the output, jump to the
	// previous/next output
	int num_outputs = root_container.children->length;
	for (i = 0; i < num_outputs; i++) {
		if (root_container.children->items[i] == current_output) {
			struct sway_container *next_output = root_container.children->items[
				wrap(i + offset, num_outputs)];
			return workspace_output_prev_next_impl(next_output, next);
		}
	}

	// Doesn't happen, at worst the for loop returns the previously active
	// workspace on the active output
	return NULL;
}

struct sway_container *workspace_output_next(struct sway_container *current) {
	return workspace_output_prev_next_impl(current, true);
}

struct sway_container *workspace_next(struct sway_container *current) {
	return workspace_prev_next_impl(current, true);
}

struct sway_container *workspace_output_prev(struct sway_container *current) {
	return workspace_output_prev_next_impl(current, false);
}

struct sway_container *workspace_prev(struct sway_container *current) {
	return workspace_prev_next_impl(current, false);
}

bool workspace_switch(struct sway_container *workspace) {
	if (!workspace) {
		return false;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, &root_container);
	if (!seat || !focus) {
		return false;
	}
	struct sway_container *active_ws = focus;
	if (active_ws->type != C_WORKSPACE) {
		active_ws = container_parent(focus, C_WORKSPACE);
	}

	if (config->auto_back_and_forth
			&& active_ws == workspace
			&& prev_workspace_name) {
		struct sway_container *new_ws = workspace_by_name(prev_workspace_name);
		workspace = new_ws ?
			new_ws :
			workspace_create(NULL, prev_workspace_name);
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

	wlr_log(L_DEBUG, "Switching to workspace %p:%s",
		workspace, workspace->name);
	struct sway_container *next = seat_get_focus_inactive(seat, workspace);
	if (next == NULL) {
		next = workspace;
	}
	seat_set_focus(seat, next);
	struct sway_container *output = container_parent(workspace, C_OUTPUT);
	arrange_output(output);
	return true;
}

bool workspace_is_visible(struct sway_container *ws) {
	struct sway_container *output = container_parent(ws, C_OUTPUT);
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus_inactive(seat, output);
	if (focus->type != C_WORKSPACE) {
		focus = container_parent(focus, C_WORKSPACE);
	}
	return focus == ws;
}
