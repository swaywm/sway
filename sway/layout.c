#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "list.h"
#include "layout.h"

list_t *outputs;

void init_layout() {
	outputs = create_list();
}

struct sway_container *get_container(wlc_handle output, int *index) {
	int i;
	for (i = 0; i < outputs->length; ++i) {
		struct sway_container *c = outputs->items[i];
		if (c->output == output) {
			return c;
		}
	}
	return NULL;
}

void add_output(wlc_handle output) {
	struct sway_container *container = malloc(sizeof(struct sway_container));
	// TODO: Get default layout from config
	container->output = output;
	container->children = create_list();
	container->layout = LAYOUT_TILE_HORIZ;
	list_add(outputs, container);
}

void destroy_output(wlc_handle output) {
	int index;
	struct sway_container *c = get_container(output, &index);
	// TODO: Move all windows in this output somewhere else?
	// I don't think this will ever be called unless we destroy the output ourselves
	if (!c) {
		return;
	}
	list_del(outputs, index);
}

wlc_handle get_topmost(wlc_handle output, size_t offset) {
   size_t memb;
   const wlc_handle *views = wlc_output_get_views(output, &memb);
   return (memb > 0 ? views[(memb - 1 + offset) % memb] : 0);
}
