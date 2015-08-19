#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "workspace.h"
#include "layout.h"
#include "list.h"
#include "log.h"
#include "container.h"
#include "handlers.h"
#include "config.h"
#include "stringop.h"
#include "focus.h"

swayc_t *active_workspace = NULL;

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
				continue;
		   
			//Make sure that the workspace doesn't already exist 
			if (workspace_find_by_name(target)) {
			   continue; 
			}

			list_free(args);

			sway_log(L_DEBUG, "Workspace: Found free name %s", target);
			return target;
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

swayc_t *workspace_create(const char* name) {
	swayc_t *parent = get_focused_container(&root_container);
	while (parent->type != C_OUTPUT) {
		parent = parent->parent;
	}
	return new_workspace(parent, name);
}

bool workspace_by_name(swayc_t *view, void *data) {
	return (view->type == C_WORKSPACE) && 
		   (strcasecmp(view->name, (char *) data) == 0);
}

swayc_t *workspace_find_by_name(const char* name) {
	return find_container(&root_container, workspace_by_name, (void *) name);
}

void workspace_output_next() {
	// Get the index of the workspace in the current output, and change the view to index+1 workspace.
	// if we're currently focused on the last workspace in the output, switch to the first
	swayc_t *current_output = active_workspace->parent;
	int i;
	for (i = 0; i < current_output->children->length - 1; i++) {
		if (strcmp((((swayc_t *)current_output->children->items[i])->name), active_workspace->name) == 0) {
			workspace_switch(current_output->children->items[i + 1]);
			return;
		}
	}
	workspace_switch(current_output->children->items[0]);
}

void workspace_next() {
	// Get the index of the workspace in the current output, and change the view to index+1 workspace.
	// if we're currently focused on the last workspace in the output, change focus to there
	// and call workspace_output_next(), as long as another output actually exists
	swayc_t *current_output = active_workspace->parent;
	int i;
	for (i = 0; i < current_output->children->length - 1; i++) {
		if (strcmp((((swayc_t *)current_output->children->items[i])->name), active_workspace->name) == 0) {
			workspace_switch(current_output->children->items[i + 1]);
			return;
		}
	}
	if (root_container.children->length > 1) {
		for (i = 0; i < root_container.children->length - 1; i++) {
			if (root_container.children->items[i] == current_output) {
				workspace_switch(((swayc_t *)root_container.children->items[i + 1])->focused);
				workspace_output_next();
				return;
			}
		}
		// If we're at the last output, then go to the first
		workspace_switch(((swayc_t *)root_container.children->items[0])->focused);
		workspace_output_next();
		return;
	} else {
		workspace_switch(current_output->children->items[0]);
	}
}

void workspace_output_prev() {
	// Get the index of the workspace in the current output, and change the view to index+1 workspace
	// if we're currently focused on the first workspace in the output, do nothing and return false
	swayc_t *current_output = active_workspace->parent;
	int i;
	for (i = 1; i < current_output->children->length; i++) {
		if (strcmp((((swayc_t *)current_output->children->items[i])->name), active_workspace->name) == 0) {
			workspace_switch(current_output->children->items[i - 1]);
			return;
		}
	}
	workspace_switch(current_output->children->items[current_output->children->length - 1]);
}

void workspace_prev() {
	// Get the index of the workspace in the current output, and change the view to index-1 workspace.
	// if we're currently focused on the last workspace in the output, change focus to there
	// and call workspace_output_next(), as long as another output actually exists

	swayc_t *current_output = active_workspace->parent;
	int i;
	for (i = 1; i < current_output->children->length; i++) {
		if (strcmp((((swayc_t *)current_output->children->items[i])->name), active_workspace->name) == 0) {
			workspace_switch(current_output->children->items[i - 1]);
			return;
		}
	}
	if (root_container.children->length > 1) {
		for (i = 1; i < root_container.children->length; i++) {
			if (root_container.children->items[i] == current_output) {
				workspace_switch(((swayc_t *)root_container.children->items[i - 1])->focused);
				workspace_output_next();
				return;
			}
		}
		// If we're at the first output, then go to the last
		workspace_switch(((swayc_t *)root_container.children->items[root_container.children->length-1])->focused);
		workspace_output_next();
		return;
	} else {
		workspace_switch(current_output->children->items[current_output->children->length - 1]);
	}

}

void workspace_switch(swayc_t *workspace) {
	if (!workspace) {
		return;
	}
	sway_log(L_DEBUG, "Switching to workspace %p:%s", workspace, workspace->name);
	active_workspace = workspace;
	set_focused_container(get_focused_view(workspace));
	arrange_windows(workspace, -1, -1);
}

/* XXX:DEBUG:XXX */
static void container_log(const swayc_t *c) {
	fprintf(stderr, "focus:%c|",
			c->is_focused ? 'F' : //Focused
			c == active_workspace ? 'W' : //active workspace
			c == &root_container  ? 'R' : //root
			'X');//not any others
	fprintf(stderr,"(%p)",c);
	fprintf(stderr,"(p:%p)",c->parent);
	fprintf(stderr,"(f:%p)",c->focused);
	fprintf(stderr,"(h:%ld)",c->handle);
	fprintf(stderr,"Type:");
	fprintf(stderr,
			c->type == C_ROOT   ? "Root|" :
			c->type == C_OUTPUT ? "Output|" :
			c->type == C_WORKSPACE ? "Workspace|" :
			c->type == C_CONTAINER ? "Container|" :
			c->type == C_VIEW   ? "View|" : "Unknown|");
	fprintf(stderr,"layout:");
	fprintf(stderr,
			c->layout == L_NONE ? "NONE|" :
			c->layout == L_HORIZ ? "Horiz|":
			c->layout == L_VERT ? "Vert|":
			c->layout == L_STACKED  ? "Stacked|":
			c->layout == L_FLOATING ? "Floating|":
			"Unknown|");
	fprintf(stderr, "w:%d|h:%d|", c->width, c->height);
	fprintf(stderr, "x:%d|y:%d|", c->x, c->y);
	fprintf(stderr, "vis:%c|", c->visible?'t':'f');
	fprintf(stderr, "wgt:%d|", c->weight);
	fprintf(stderr, "name:%.16s|", c->name);
	fprintf(stderr, "children:%d\n",c->children?c->children->length:0);
}
void layout_log(const swayc_t *c, int depth) {
	int i, d;
	int e = c->children ? c->children->length : 0;
	container_log(c);
	if (e) {
		for (i = 0; i < e; ++i) {
			fputc('|',stderr);
			for (d = 0; d < depth; ++d) fputc('-', stderr);
			layout_log(c->children->items[i], depth + 1);
		}
	}
	if (c->type == C_WORKSPACE) {
		e = c->floating?c->floating->length:0;
		if (e) {
			for (i = 0; i < e; ++i) {
				fputc('|',stderr);
				for (d = 0; d < depth; ++d) fputc('-', stderr);
				layout_log(c->floating->items[i], depth + 1);
			}
		}
	}
}
/* XXX:DEBUG:XXX */
