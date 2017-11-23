#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"
#include "sway/workspace.h"
#include "log.h"

static swayc_t *new_swayc(enum swayc_types type) {
	// next id starts at 1 because 0 is assigned to root_container in layout.c
	static size_t next_id = 1;
	swayc_t *c = calloc(1, sizeof(swayc_t));
	if (!c) {
		return NULL;
	}
	c->id = next_id++;
	c->layout = L_NONE;
	c->workspace_layout = L_NONE;
	c->type = type;
	c->nb_master = 1;
	c->nb_slave_groups = 1;
	if (type != C_VIEW) {
		c->children = create_list();
	}
	return c;
}

swayc_t *new_output(struct sway_output *sway_output) {
	struct wlr_box size;
	wlr_output_effective_resolution(
			sway_output->wlr_output, &size.width, &size.height);
	const char *name = sway_output->wlr_output->name;

	swayc_t *output = new_swayc(C_OUTPUT);
	output->sway_output = sway_output;
	output->name = name ? strdup(name) : NULL;
	output->width = size.width;
	output->height = size.width;

	add_child(&root_container, output);

	// Create workspace
	char *ws_name = workspace_next_name(output->name);
	sway_log(L_DEBUG, "Creating default workspace %s", ws_name);
	new_workspace(output, ws_name);
	free(ws_name);
	return output;
}

swayc_t *new_workspace(swayc_t *output, const char *name) {
	if (!sway_assert(output, "new_workspace called with null output")) {
		return NULL;
	}
	sway_log(L_DEBUG, "Added workspace %s for output %s", name, output->name);
	swayc_t *workspace = new_swayc(C_WORKSPACE);

	workspace->x = output->x;
	workspace->y = output->y;
	workspace->width = output->width;
	workspace->height = output->height;
	workspace->name = !name ? NULL : strdup(name);
	workspace->prev_layout = L_NONE;
	workspace->layout = default_layout(output);
	workspace->workspace_layout = default_layout(output);

	add_child(output, workspace);
	sort_workspaces(output);
	return workspace;
}
