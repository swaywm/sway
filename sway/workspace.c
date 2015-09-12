#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <string.h>
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

char *prev_workspace_name = NULL;

static swayc_t *workspace_by_name_only(const char *name);

const char *workspace_output_open_name(swayc_t *output) {
	struct workspace_output *wsop;
	int i, len = config->workspace_outputs->length;
	// Search config for output
	for (i = 0; i < len; ++i) {
		wsop = config->workspace_outputs->items[i];
		// Find matching outputs
		if (strcasecmp(wsop->output, output->name)) {
			// Check if workspace is available and use that name
			if (!workspace_by_name(wsop->workspace)) {
				return wsop->workspace;
			}
		}
	}
	return NULL;
}

const char *workspace_next_name(void) {
	sway_log(L_DEBUG, "Workspace: Generating new name");
	int i;
	int l = 1;
	// Scan all workspace bindings to find the next available workspace name,
	// if none are found/available then default to a number
	struct sway_mode *mode = config->current_mode;

	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];
		const char *command = binding->command;
		const char *ws = "workspace";
		const int wslen = sizeof("workspace") - 1;
		if (strncmp(ws, command, wslen) == 0) {
			command += wslen;
			// Skip whitespace
			command += strspn(command, whitespace);
			// make sure its not a special command
			if (strcmp(command, "next") == 0
					|| strcmp(command, "prev") == 0
					|| strcmp(command, "next_on_output") == 0
					|| strcmp(command, "prev_on_output") == 0
					|| strcmp(command, "number") == 0
					|| strcmp(command, "back_and_forth") == 0
					|| strcmp(command, "current") == 0
					// Or if it already exists
					|| workspace_by_name_only(command)) {
				continue;
			} else {
				// otherwise we found it
				return command;
			}
		}
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

swayc_t *workspace_by_name_only(const char *name) {
	int i, len = root_container.children->length;
	for (i = 0; i < len; ++i) {
		swayc_t *op = root_container.children->items[i];
		int i, len = op->children->length;
		for (i = 0; i < len; ++i) {
			swayc_t *ws = op->children->items[i];
			if (strcasecmp(ws->name, name) == 0) {
				return ws;
			}
		}
	}
	return NULL;
}

swayc_t *workspace_by_name(const char* name) {
	if (strcmp(name, "prev") == 0) {
		return workspace_prev();
	} else if (!strcmp(name, "prev_on_output")) {
		return workspace_output_prev();
	} else if (!strcmp(name, "next")) {
		return workspace_next();
	} else if (!strcmp(name, "next_on_output")) {
		return workspace_output_next();
	} else if (!strcmp(name, "current")) {
		return swayc_active_workspace();
	} else if (!strcmp(name, "back_and_forth")) {
		if (prev_workspace_name) {
			name = prev_workspace_name;
		} else { // If there is no prev workspace name. just return current
			return swayc_active_workspace();
		}
	}
	return workspace_by_name_only(name);
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

swayc_t *workspace_output_next(void) {
	return workspace_output_prev_next_impl(swayc_active_output(), true);
}

swayc_t *workspace_next(void) {
	return workspace_prev_next_impl(swayc_active_workspace(), true);
}

swayc_t *workspace_output_prev(void) {
	return workspace_output_prev_next_impl(swayc_active_output(), false);
}

swayc_t *workspace_prev(void) {
	return workspace_prev_next_impl(swayc_active_workspace(), false);
}

void workspace_switch(swayc_t *workspace) {
	if (!workspace) {
		return;
	}
	swayc_t *active_ws = swayc_active_workspace();
	// set workspace to prev_workspace
	if (config->auto_back_and_forth && active_ws == workspace) {
		workspace = new_workspace(NULL, "back_and_forth");
	}

	// set prev workspace name
	if (!prev_workspace_name
			|| (strcmp(prev_workspace_name, active_ws->name)
				&& active_ws != workspace)) {
		free(prev_workspace_name);
		prev_workspace_name = strdup(active_ws->name);
	}

	sway_log(L_DEBUG, "Switching to workspace %p:%s", workspace, workspace->name);
	set_focused_container(get_focused_view(workspace));
	arrange_windows(workspace, -1, -1);
}
