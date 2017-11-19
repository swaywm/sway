#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"

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
	wlr_output_effective_resolution(sway_output->wlr_output,
			&size.width, &size.height);
	const char *name = sway_output->wlr_output->name;

	swayc_t *output = new_swayc(C_OUTPUT);
	output->sway_output = sway_output;
	output->name = name ? strdup(name) : NULL;
	output->width = size.width;
	output->height = size.width;

	add_child(&root_container, output);

	// TODO: Create workspace

	return output;
}
