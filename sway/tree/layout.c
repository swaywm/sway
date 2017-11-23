#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
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

enum swayc_layouts default_layout(swayc_t *output) {
	/* TODO WLR
	if (config->default_layout != L_NONE) {
		//return config->default_layout;
	} else if (config->default_orientation != L_NONE) {
		return config->default_orientation;
	} else */if (output->width >= output->height) {
		return L_HORIZ;
	} else {
		return L_VERT;
	}
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	swayc_t *a = *(void **)_a;
	swayc_t *b = *(void **)_b;
	int retval = 0;

	if (isdigit(a->name[0]) && isdigit(b->name[0])) {
		int a_num = strtol(a->name, NULL, 10);
		int b_num = strtol(b->name, NULL, 10);
		retval = (a_num < b_num) ? -1 : (a_num > b_num);
	} else if (isdigit(a->name[0])) {
		retval = -1;
	} else if (isdigit(b->name[0])) {
		retval = 1;
	}

	return retval;
}

void sort_workspaces(swayc_t *output) {
	list_stable_sort(output->children, sort_workspace_cmp_qsort);
}
