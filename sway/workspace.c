#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <wlc/wlc.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include "ipc-server.h"
#include "extensions.h"
#include "workspace.h"
#include "layout.h"
#include "list.h"
#include "log.h"
#include "container.h"
#include "handlers.h"
#include "config.h"
#include "stringop.h"
#include "focus.h"
#include "util.h"
#include "ipc.h"

char *prev_workspace_name = NULL;
struct workspace_by_number_data {
	int len;
	const char *cset;
	const char *name;
};

static bool workspace_valid_on_output(const char *output_name, const char *ws_name) {
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
	sway_log(L_DEBUG, "Workspace: Generating new workspace name for output %s", output_name);
	int i;
	int l = 1;
	// Scan all workspace bindings to find the next available workspace name,
	// if none are found/available then default to a number
	struct sway_mode *mode = config->current_mode;

	int order = INT_MAX;
	char *target = NULL;
	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];
		char *cmdlist = strdup(binding->command);
		char *dup = cmdlist;
		char *name = NULL;

		// workspace n
		char *cmd = argsep(&cmdlist, " ");
		if (cmdlist) {
			name = argsep(&cmdlist, " ,;");
		}

		if (strcmp("workspace", cmd) == 0 && name) {
			sway_log(L_DEBUG, "Got valid workspace command for target: '%s'", name);
			char *_target = strdup(name);
			strip_quotes(_target);
			while (isspace(*_target))
				_target++;

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
				sway_log(L_DEBUG, "Workspace: Found free name %s", _target);
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
	sprintf(name, "%d", ws_num++);
	return name;
}

swayc_t *workspace_create(const char* name) {
	swayc_t *parent;
	// Search for workspace<->output pair
	int i, e = config->workspace_outputs->length;
	for (i = 0; i < e; ++i) {
		struct workspace_output *wso = config->workspace_outputs->items[i];
		if (strcasecmp(wso->workspace, name) == 0)
		{
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
	parent = get_focused_container(&root_container);
	parent = swayc_parent_by_type(parent, C_OUTPUT);
	return new_workspace(parent, name);
}

static bool _workspace_by_name(swayc_t *view, void *data) {
	return (view->type == C_WORKSPACE) &&
		   (strcasecmp(view->name, (char *) data) == 0);
}

swayc_t *workspace_by_name(const char* name) {
	if (strcmp(name, "prev") == 0) {
		return workspace_prev();
	}
	else if (strcmp(name, "prev_on_output") == 0) {
		return workspace_output_prev();
	}
	else if (strcmp(name, "next") == 0) {
		return workspace_next();
	}
	else if (strcmp(name, "next_on_output") == 0) {
		return workspace_output_next();
	}
	else if (strcmp(name, "current") == 0) {
		return swayc_active_workspace();
	}
	else {
		return swayc_by_test(&root_container, _workspace_by_name, (void *) name);
	}
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

/**
 * Get the previous or next workspace on the specified output.
 * Wraps around at the end and beginning.
 * If next is false, the previous workspace is returned, otherwise the next one is returned.
 */
swayc_t *workspace_output_prev_next_impl(swayc_t *output, bool next) {
	if (!sway_assert(output->type == C_OUTPUT, "Argument must be an output, is %d", output->type)) {
		return NULL;
	}

	int i;
	for (i = 0; i < output->children->length; i++) {
		if (output->children->items[i] == output->focused) {
			return output->children->items[wrap(i + (next ? 1 : -1), output->children->length)];
		}
	}

	// Doesn't happen, at worst the for loop returns the previously active workspace
	return NULL;
}

/**
 * Get the previous or next workspace. If the first/last workspace on an output is active,
 * proceed to the previous/next output's previous/next workspace.
 * If next is false, the previous workspace is returned, otherwise the next one is returned.
 */
swayc_t *workspace_prev_next_impl(swayc_t *workspace, bool next) {
	if (!sway_assert(workspace->type == C_WORKSPACE, "Argument must be a workspace, is %d", workspace->type)) {
		return NULL;
	}

	swayc_t *current_output = workspace->parent;
	int offset = next ? 1 : -1;
	int start = next ? 0 : 1;
	int end = next ? (current_output->children->length) - 1 : current_output->children->length;
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
			swayc_t *next_output = root_container.children->items[wrap(i + offset, num_outputs)];
			return workspace_output_prev_next_impl(next_output, next);
		}
	}

	// Doesn't happen, at worst the for loop returns the previously active workspace on the active output
	return NULL;
}

swayc_t *workspace_output_next() {
	return workspace_output_prev_next_impl(swayc_active_output(), true);
}

swayc_t *workspace_next() {
	return workspace_prev_next_impl(swayc_active_workspace(), true);
}

swayc_t *workspace_output_prev() {
	return workspace_output_prev_next_impl(swayc_active_output(), false);
}

swayc_t *workspace_prev() {
	return workspace_prev_next_impl(swayc_active_workspace(), false);
}

bool workspace_switch(swayc_t *workspace) {
	if (!workspace) {
		return false;
	}
	swayc_t *active_ws = swayc_active_workspace();
	if (config->auto_back_and_forth && active_ws == workspace && prev_workspace_name) {
		swayc_t *new_ws = workspace_by_name(prev_workspace_name);
		workspace = new_ws ? new_ws : workspace_create(prev_workspace_name);
	}

	if (!prev_workspace_name
			|| (strcmp(prev_workspace_name, active_ws->name)
				&& active_ws != workspace)) {
		free(prev_workspace_name);
		prev_workspace_name = malloc(strlen(active_ws->name)+1);
		strcpy(prev_workspace_name, active_ws->name);
	}

	// move sticky containers
	if (swayc_parent_by_type(active_ws, C_OUTPUT) == swayc_parent_by_type(workspace, C_OUTPUT)) {
		// don't change list while traversing it, use intermediate list instead
		list_t *stickies = create_list();
		for (int i = 0; i < active_ws->floating->length; i++) {
			swayc_t *cont = active_ws->floating->items[i];
			if (cont->sticky) {
				list_add(stickies, cont);
			}
		}
		for (int i = 0; i < stickies->length; i++) {
			swayc_t *cont = stickies->items[i];
			sway_log(L_DEBUG, "Moving sticky container %p to %p:%s",
					cont, workspace, workspace->name);
			swayc_t *parent = remove_child(cont);
			add_floating(workspace, cont);
			// Destroy old container if we need to
			destroy_container(parent);
		}
		list_free(stickies);
	}
	sway_log(L_DEBUG, "Switching to workspace %p:%s", workspace, workspace->name);
	if (!set_focused_container(get_focused_view(workspace))) {
		return false;
	}
	swayc_t *output = swayc_parent_by_type(workspace, C_OUTPUT);
	arrange_windows(output, -1, -1);
	return true;
}

swayc_t *workspace_for_pid(pid_t pid) {
	int i;
	swayc_t *ws = NULL;
	struct pid_workspace *pw = NULL;

	sway_log(L_DEBUG, "looking for workspace for pid %d", pid);

	// leaving this here as it's useful for debugging
	// sway_log(L_DEBUG, "all pid_workspaces");
	// for (int k = 0; k < config->pid_workspaces->length; k++) {
	// 	pw = config->pid_workspaces->items[k];
	// 	sway_log(L_DEBUG, "pid %d workspace %s time_added %li", *pw->pid, pw->workspace, *pw->time_added);
	// }

	do {
		for (i = 0; i < config->pid_workspaces->length; i++) {
			pw = config->pid_workspaces->items[i];
			pid_t *pw_pid = pw->pid;

			if (pid == *pw_pid) {
				sway_log(L_DEBUG, "found pid_workspace for pid %d, workspace %s", pid, pw->workspace);
				break; // out of for loop
			}

			pw = NULL;
		}

		if (pw) {
			break; // out of do-while loop
		}

		pid = get_parent_pid(pid);
		// no sense in looking for matches for pid 0.
		// also, if pid == getpid(), that is the compositor's
		// pid, which definitely isn't helpful
	} while (pid > 0 && pid != getpid());

	if (pw) {
		ws = workspace_by_name(pw->workspace);

		if (!ws) {
			sway_log(L_DEBUG, "Creating workspace %s for pid %d because it disappeared", pw->workspace, pid);
			ws = workspace_create(pw->workspace);
		}

		list_del(config->pid_workspaces, i);
	}

	return ws;
}
