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

char *workspace_next_name(void) {
	sway_log(L_DEBUG, "Workspace: Generating new name");
	int i;
	int l = 1;
	// Scan all workspace bindings to find the next available workspace name,
	// if none are found/available then default to a number
	struct sway_mode *mode = config->current_mode;

	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];
		const char* command = binding->command;
		list_t *args = split_string(command, " ");

		if (strcmp("workspace", args->items[0]) == 0 && args->length > 1) {
			sway_log(L_DEBUG, "Got valid workspace command for target: '%s'", (char *)args->items[1]);
			char* target = malloc(strlen(args->items[1]) + 1);
			strcpy(target, args->items[1]);
			while (*target == ' ' || *target == '\t')
				target++;

			// Make sure that the command references an actual workspace
			// not a command about workspaces
			if (strcmp(target, "next") == 0 ||
				strcmp(target, "prev") == 0 ||
				strcmp(target, "next_on_output") == 0 ||
				strcmp(target, "prev_on_output") == 0 ||
				strcmp(target, "number") == 0 ||
				strcmp(target, "back_and_forth") == 0 ||
				strcmp(target, "current") == 0)
			{
				free_flat_list(args);
				continue;
			}

			// Make sure that the workspace doesn't already exist
			if (workspace_by_name(target)) {
				free_flat_list(args);
				continue;
			}

			free_flat_list(args);

			sway_log(L_DEBUG, "Workspace: Found free name %s", target);
			return target;
		}
		free_flat_list(args);
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
	swayc_t *parent = get_focused_container(&root_container);
	parent = swayc_parent_by_type(parent, C_OUTPUT);
	return new_workspace(parent, name);
}

static bool _workspace_by_name(swayc_t *view, void *data) {
	return (view->type == C_WORKSPACE) &&
		   (strcasecmp(view->name, (char *) data) == 0);
}

swayc_t *workspace_by_name(const char* name) {
	return swayc_by_test(&root_container, _workspace_by_name, (void *) name);
}

void workspace_output_next() {
	// Get the index of the workspace in the current output, and change the view to index+1 workspace.
	// if we're currently focused on the last workspace in the output, switch to the first
	swayc_t *current_output = swayc_active_workspace()->parent;
	int i;
	for (i = 0; i < current_output->children->length - 1; i++) {
		if (strcmp((((swayc_t *)current_output->children->items[i])->name), swayc_active_workspace()->name) == 0) {
			workspace_switch(current_output->children->items[i + 1]);
			return;
		}
	}
	workspace_switch(current_output->children->items[0]);
}

void workspace_next() {
	// Get the index of the workspace in the current output, and change the focus to index+1 workspace.
	// if we're currently focused on the last workspace in the output, change focus to the next output
	// and call workspace_output_next()

	swayc_t *current_output = swayc_active_workspace()->parent;
	int i;
	for (i = 0; i < current_output->children->length - 1; i++) {
		if (current_output->children->items[i] == swayc_active_workspace()) {
			workspace_switch(current_output->children->items[i + 1]);
			return;
		}
	}

	int num_outputs = root_container.children->length;
	for (i = 0; i < num_outputs; i++) {
		if (root_container.children->items[i] == current_output) {
			swayc_t *next_output = root_container.children->items[wrap(++i, num_outputs)];
			workspace_switch(next_output->focused);
			workspace_output_next();
			return;
		}
	}
}

void workspace_output_prev() {
	// Get the index of the workspace in the current output, and change the view to index+1 workspace
	// if we're currently focused on the first workspace in the output, do nothing and return false
	swayc_t *current_output = swayc_active_workspace()->parent;
	int i;
	for (i = 1; i < current_output->children->length; i++) {
		if (strcmp((((swayc_t *)current_output->children->items[i])->name), swayc_active_workspace()->name) == 0) {
			workspace_switch(current_output->children->items[i - 1]);
			return;
		}
	}
	workspace_switch(current_output->children->items[current_output->children->length - 1]);
}

void workspace_prev() {
	// Get the index of the workspace in the current output, and change the focus to index-1 workspace.
	// if we're currently focused on the first workspace in the output, change focus to the previous output
	// and call workspace_output_prev()

	swayc_t *current_output = swayc_active_workspace()->parent;
	int i;
	for (i = 1; i < current_output->children->length; i++) {
		if (current_output->children->items[i] == swayc_active_workspace()) {
			workspace_switch(current_output->children->items[i - 1]);
			return;
		}
	}

	int num_outputs = root_container.children->length;
	for (i = 0; i < num_outputs; i++) {
		if (root_container.children->items[i] == current_output) {
			swayc_t *prev_output = root_container.children->items[wrap(--i, num_outputs)];
			workspace_switch(prev_output->focused);
			workspace_output_prev();
			return;
		}
	}
}

void workspace_switch(swayc_t *workspace) {
	if (!workspace) {
		return;
	}
	sway_log(L_DEBUG, "Switching to workspace %p:%s", workspace, workspace->name);
	set_focused_container(get_focused_view(workspace));
	arrange_windows(workspace, -1, -1);
}
