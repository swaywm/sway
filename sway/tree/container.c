#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/config.h"
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"
#include "sway/view.h"
#include "sway/workspace.h"
#include "log.h"

void swayc_descendants_of_type(swayc_t *root, enum swayc_types type,
		void (*func)(swayc_t *item, void *data), void *data) {
	for (int i = 0; i < root->children->length; ++i) {
		swayc_t *item = root->children->items[i];
		if (item->type == type) {
			func(item, data);
		}
		if (item->children && item->children->length) {
			swayc_descendants_of_type(item, type, func, data);
		}
	}
}

static void update_root_geometry() {
	struct wlr_box *box =
		wlr_output_layout_get_box(root_container.output_layout, NULL);
	root_container.width = box->width;
	root_container.height = box->height;
}

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
	wlr_output_effective_resolution(sway_output->wlr_output, &size.width,
		&size.height);
	const char *name = sway_output->wlr_output->name;

	struct output_config *oc = NULL, *all = NULL;
	for (int i = 0; i < config->output_configs->length; ++i) {
		struct output_config *cur = config->output_configs->items[i];
		if (strcasecmp(name, cur->name) == 0) {
			sway_log(L_DEBUG, "Matched output config for %s", name);
			oc = cur;
		}
		if (strcasecmp("*", cur->name) == 0) {
			sway_log(L_DEBUG, "Matched wildcard output config for %s", name);
			all = cur;
		}

		if (oc && all) {
			break;
		}
	}
	if (!oc) {
		oc = all;
	}
	if (oc && !oc->enabled) {
		return NULL;
	}

	swayc_t *output = new_swayc(C_OUTPUT);
	output->sway_output = sway_output;
	output->name = name ? strdup(name) : NULL;

	apply_output_config(oc, output);

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

swayc_t *new_view(swayc_t *sibling, struct sway_view *sway_view) {
	if (!sway_assert(sibling, "new_view called with NULL sibling/parent")) {
		return NULL;
	}
	const char *title = sway_view->iface.get_prop(sway_view, VIEW_PROP_TITLE);
	swayc_t *swayc = new_swayc(C_VIEW);
	sway_log(L_DEBUG, "Adding new view %p:%s to container %p %d",
		swayc, title, sibling, sibling ? sibling->type : 0);
	// Setup values
	swayc->sway_view = sway_view;
	swayc->name = title ? strdup(title) : NULL;
	swayc->width = 0;
	swayc->height = 0;

	if (sibling->type == C_WORKSPACE) {
		// Case of focused workspace, just create as child of it
		add_child(sibling, swayc);
	} else {
		// Regular case, create as sibling of current container
		// TODO WLR
		//add_sibling(sibling, swayc);
	}
	return swayc;
}

static void free_swayc(swayc_t *cont) {
	if (!sway_assert(cont, "free_swayc passed NULL")) {
		return;
	}
	if (cont->children) {
		// remove children until there are no more, free_swayc calls
		// remove_child, which removes child from this container
		while (cont->children->length) {
			free_swayc(cont->children->items[0]);
		}
		list_free(cont->children);
	}
	if (cont->marks) {
		list_foreach(cont->marks, free);
		list_free(cont->marks);
	}
	if (cont->parent) {
		remove_child(cont);
	}
	if (cont->name) {
		free(cont->name);
	}
	free(cont);
}

swayc_t *destroy_output(swayc_t *output) {
	if (!sway_assert(output, "null output passed to destroy_output")) {
		return NULL;
	}
	if (output->children->length > 0) {
		// TODO save workspaces when there are no outputs.
		// TODO also check if there will ever be no outputs except for exiting
		// program
		if (root_container.children->length > 1) {
			int p = root_container.children->items[0] == output;
			// Move workspace from this output to another output
			while (output->children->length) {
				swayc_t *child = output->children->items[0];
				remove_child(child);
				add_child(root_container.children->items[p], child);
			}
			sort_workspaces(root_container.children->items[p]);
			// TODO WLR: is this needed anymore?
			//update_visibility(root_container.children->items[p]);
			arrange_windows(root_container.children->items[p], -1, -1);
		}
	}
	sway_log(L_DEBUG, "OUTPUT: Destroying output '%s'", output->name);
	free_swayc(output);
	update_root_geometry();
	return &root_container;
}

swayc_t *destroy_view(swayc_t *view) {
	if (!sway_assert(view, "null view passed to destroy_view")) {
		return NULL;
	}
	sway_log(L_DEBUG, "Destroying view '%s'", view->name);
	swayc_t *parent = view->parent;
	free_swayc(view);

	// TODO WLR: Destroy empty containers
	/*
	if (parent && parent->type == C_CONTAINER) {
		return destroy_container(parent);
	}
	*/
	return parent;
}

swayc_t *swayc_parent_by_type(swayc_t *container, enum swayc_types type) {
	if (!sway_assert(container, "container is NULL")) {
		return NULL;
	}
	if (!sway_assert(type < C_TYPES && type >= C_ROOT, "invalid type")) {
		return NULL;
	}
	do {
		container = container->parent;
	} while (container && container->type != type);
	return container;
}
