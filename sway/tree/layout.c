#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <string.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/container.h"
#include "list.h"
#include "log.h"

swayc_t root_container;

void init_layout(void) {
	root_container.id = 0; // normally assigned in new_swayc()
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.name = strdup("root");
	root_container.children = create_list();
	root_container.output_layout = wlr_output_layout_create();
}

void add_child(swayc_t *parent, swayc_t *child) {
	sway_log(L_DEBUG, "Adding %p (%d, %fx%f) to %p (%d, %fx%f)",
			child, child->type, child->width, child->height,
			parent, parent->type, parent->width, parent->height);
	list_add(parent->children, child);
	child->parent = parent;
	// set focus for this container
	if (!parent->focused) {
		parent->focused = child;
	}
	/* TODO WLR
	if (parent->type == C_WORKSPACE && child->type == C_VIEW && (parent->workspace_layout == L_TABBED || parent->workspace_layout == L_STACKED)) {
		child = new_container(child, parent->workspace_layout);
	}
	*/
}
