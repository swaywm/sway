#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "workspace.h"
#include "layout.h"
#include "list.h"
#include "log.h"
#include "container.h"

swayc_t *active_workspace = NULL;

char *workspace_next_name(void) {
	//TODO change this i guess. seems pretty bad
	char *name = malloc(sizeof("1"));
	return strcpy(name, "1");
}

swayc_t *workspace_create(const char* name) {
	swayc_t *parent = get_focused_container(&root_container);
	while(parent->type != C_OUTPUT) {
		parent = parent->parent;
	}

	swayc_t *workspace = create_container(parent, -1);
	workspace->type = C_WORKSPACE;
	workspace->name = strdup(name);
	workspace->width = parent->width;
	workspace->height = parent->height;
	workspace->layout = L_HORIZ; // todo: thing
	
	add_child(parent, workspace);
	return workspace;
}

bool workspace_by_name(swayc_t *view, void *data) {
	return (view->type == C_WORKSPACE) && 
		   (strcasecmp(view->name, (char *) data) == 0);
}

bool workspace_destroy(swayc_t *workspace) {
	//Dont destroy if there are children
	if(workspace->children->length) {
		return false;
	}
	sway_log(L_DEBUG, "Workspace: Destroying workspace '%s'", workspace->name);
	free_swayc(workspace);
	return true;

}

void set_mask(swayc_t *view, void *data) {
	uint32_t *p = data;

	if(view->type == C_VIEW) {
		wlc_view_set_mask(view->handle, *p);
		view->visible = (*p == 2);
	}
}

swayc_t *workspace_find_by_name(const char* name) {
	return find_container(&root_container, workspace_by_name, (void *) name);
}

void workspace_switch(swayc_t *workspace) {
	if (active_workspace) {
		sway_log(L_DEBUG, "workspace: changing from '%s' to '%s'", active_workspace->name, workspace->name);
		if(strcmp(active_workspace->name, workspace->name) == 0) {
			return; //Dont do anything if they are the same workspace
		}
		uint32_t mask = 1;
		// set all c_views in the old workspace to the invisible mask
		container_map(active_workspace, set_mask, &mask);

		// and c_views in the new workspace to the visible mask
		mask = 2;
		container_map(workspace, set_mask, &mask);

		wlc_output_set_mask(wlc_get_focused_output(), 2);
		unfocus_all(active_workspace);
		focus_view(workspace);
		workspace_destroy(active_workspace);
	}
	active_workspace = workspace;
}
