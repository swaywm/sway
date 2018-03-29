#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_wl_shell.h>
#include "sway/config.h"
#include "sway/tree/container.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/ipc-server.h"
#include "log.h"

static list_t *bfs_queue;

static list_t *get_bfs_queue() {
	if (!bfs_queue) {
		bfs_queue = create_list();
		if (!bfs_queue) {
			wlr_log(L_ERROR, "could not allocate list for bfs queue");
			return NULL;
		}
	}
	bfs_queue->length = 0;

	return bfs_queue;
}

static void notify_new_container(struct sway_container *container) {
	wl_signal_emit(&root_container.sway_root->events.new_container, container);
	ipc_event_window(container, "new");
}

static struct sway_container *new_swayc(enum sway_container_type type) {
	// next id starts at 1 because 0 is assigned to root_container in layout.c
	static size_t next_id = 1;
	struct sway_container *c = calloc(1, sizeof(struct sway_container));
	if (!c) {
		return NULL;
	}
	c->id = next_id++;
	c->layout = L_NONE;
	c->workspace_layout = L_NONE;
	c->type = type;
	if (type != C_VIEW) {
		c->children = create_list();
	}

	wl_signal_init(&c->events.destroy);

	return c;
}

static void container_destroy(struct sway_container *cont) {
	if (cont == NULL) {
		return;
	}

	wl_signal_emit(&cont->events.destroy, cont);

	if (cont->children) {
		// remove children until there are no more, container_destroy calls
		// container_remove_child, which removes child from this container
		while (cont->children->length) {
			container_destroy(cont->children->items[0]);
		}
		list_free(cont->children);
	}
	if (cont->marks) {
		list_foreach(cont->marks, free);
		list_free(cont->marks);
	}
	if (cont->parent) {
		container_remove_child(cont);
	}
	if (cont->name) {
		free(cont->name);
	}
	free(cont);
}

struct sway_container *container_output_create(
		struct sway_output *sway_output) {
	struct wlr_box size;
	wlr_output_effective_resolution(sway_output->wlr_output, &size.width,
		&size.height);

	const char *name = sway_output->wlr_output->name;
	char identifier[128];
	output_get_identifier(identifier, sizeof(identifier), sway_output);

	struct output_config *oc = NULL, *all = NULL;
	for (int i = 0; i < config->output_configs->length; ++i) {
		struct output_config *cur = config->output_configs->items[i];

		if (strcasecmp(name, cur->name) == 0 ||
				strcasecmp(identifier, cur->name) == 0) {
			wlr_log(L_DEBUG, "Matched output config for %s", name);
			oc = cur;
		}
		if (strcasecmp("*", cur->name) == 0) {
			wlr_log(L_DEBUG, "Matched wildcard output config for %s", name);
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

	struct sway_container *output = new_swayc(C_OUTPUT);
	output->sway_output = sway_output;
	output->name = strdup(name);
	if (output->name == NULL) {
		container_destroy(output);
		return NULL;
	}

	apply_output_config(oc, output);

	container_add_child(&root_container, output);

	// Create workspace
	char *ws_name = workspace_next_name(output->name);
	wlr_log(L_DEBUG, "Creating default workspace %s", ws_name);
	struct sway_container *ws = container_workspace_create(output, ws_name);
	// Set each seat's focus if not already set
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input_manager->seats, link) {
		if (!seat->has_focus) {
			sway_seat_set_focus(seat, ws);
		}
	}

	free(ws_name);
	notify_new_container(output);
	return output;
}

struct sway_container *container_workspace_create(struct sway_container
		*output, const char *name) {
	if (!sway_assert(output,
				"container_workspace_create called with null output")) {
		return NULL;
	}
	wlr_log(L_DEBUG, "Added workspace %s for output %s", name, output->name);
	struct sway_container *workspace = new_swayc(C_WORKSPACE);

	workspace->x = output->x;
	workspace->y = output->y;
	workspace->width = output->width;
	workspace->height = output->height;
	workspace->name = !name ? NULL : strdup(name);
	workspace->prev_layout = L_NONE;
	workspace->layout = container_get_default_layout(output);
	workspace->workspace_layout = container_get_default_layout(output);

	container_add_child(output, workspace);
	container_sort_workspaces(output);
	notify_new_container(workspace);
	return workspace;
}

struct sway_container *container_view_create(struct sway_container *sibling,
		struct sway_view *sway_view) {
	if (!sway_assert(sibling,
				"container_view_create called with NULL sibling/parent")) {
		return NULL;
	}
	const char *title = view_get_title(sway_view);
	struct sway_container *swayc = new_swayc(C_VIEW);
	wlr_log(L_DEBUG, "Adding new view %p:%s to container %p %d %s",
		swayc, title, sibling, sibling ? sibling->type : 0, sibling->name);
	// Setup values
	swayc->sway_view = sway_view;
	swayc->name = title ? strdup(title) : NULL;
	swayc->width = 0;
	swayc->height = 0;

	if (sibling->type == C_WORKSPACE) {
		// Case of focused workspace, just create as child of it
		container_add_child(sibling, swayc);
	} else {
		// Regular case, create as sibling of current container
		container_add_sibling(sibling, swayc);
	}
	notify_new_container(swayc);
	return swayc;
}

struct sway_container *container_output_destroy(struct sway_container *output) {
	if (!sway_assert(output,
				"null output passed to container_output_destroy")) {
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
				struct sway_container *child = output->children->items[0];
				container_remove_child(child);
				container_add_child(root_container.children->items[p], child);
			}
			container_sort_workspaces(root_container.children->items[p]);
			container_arrange_windows(root_container.children->items[p],
				-1, -1);
		}
	}

	wl_list_remove(&output->sway_output->frame.link);
	wl_list_remove(&output->sway_output->destroy.link);
	wl_list_remove(&output->sway_output->mode.link);

	wlr_log(L_DEBUG, "OUTPUT: Destroying output '%s'", output->name);
	container_destroy(output);

	return &root_container;
}

struct sway_container *container_view_destroy(struct sway_container *view) {
	if (!view) {
		return NULL;
	}
	wlr_log(L_DEBUG, "Destroying view '%s'", view->name);
	struct sway_container *parent = view->parent;
	container_destroy(view);

	// TODO WLR: Destroy empty containers
	/*
	if (parent && parent->type == C_CONTAINER) {
		return destroy_container(parent);
	}
	*/
	return parent;
}

struct sway_container *container_set_layout(struct sway_container *container,
		enum sway_container_layout layout) {
	if (container->type == C_WORKSPACE) {
		container->workspace_layout = layout;
		if (layout == L_HORIZ || layout == L_VERT) {
			container->layout = layout;
		}
	} else {
		container->layout = layout;
	}
	return container;
}

void container_descendents(struct sway_container *root,
		enum sway_container_type type,
		void (*func)(struct sway_container *item, void *data), void *data) {
	for (int i = 0; i < root->children->length; ++i) {
		struct sway_container *item = root->children->items[i];
		if (item->type == type) {
			func(item, data);
		}
		if (item->children && item->children->length) {
			container_descendents(item, type, func, data);
		}
	}
}

struct sway_container *container_find(struct sway_container *container,
		bool (*test)(struct sway_container *view, void *data), void *data) {
	if (!container->children) {
		return NULL;
	}
	// TODO: floating windows
	for (int i = 0; i < container->children->length; ++i) {
		struct sway_container *child = container->children->items[i];
		if (test(child, data)) {
			return child;
		} else {
			struct sway_container *res = container_find(child, test, data);
			if (res) {
				return res;
			}
		}
	}
	return NULL;
}

struct sway_container *container_parent(struct sway_container *container,
		enum sway_container_type type) {
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

void sway_container_for_each(struct sway_container *container,
		void (*f)(struct sway_container *view, void *data), void *data) {
	if (container) {
		int i;
		if (container->children)  {
			for (i = 0; i < container->children->length; ++i) {
				struct sway_container *child = container->children->items[i];
				sway_container_for_each(child, f, data);
			}
		}
		// TODO
		/*
		if (container->floating) {
			for (i = 0; i < container->floating->length; ++i) {
				struct sway_container *child = container->floating->items[i];
				container_map(child, f, data);
			}
		}
		*/
		f(container, data);
	}
}

struct sway_container *sway_container_at(struct sway_container *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	list_t *queue = get_bfs_queue();
	if (!queue) {
		return NULL;
	}

	list_add(queue, parent);

	struct sway_container *swayc = NULL;
	while (queue->length) {
		swayc = queue->items[0];
		list_del(queue, 0);
		if (swayc->type == C_VIEW) {
			struct sway_view *sview = swayc->sway_view;
			struct sway_container *soutput = container_parent(swayc, C_OUTPUT);
			struct wlr_box *output_box =
				wlr_output_layout_get_box(
					root_container.sway_root->output_layout,
					soutput->sway_output->wlr_output);
			double ox = lx - output_box->x;
			double oy = ly - output_box->y;
			double view_sx = ox - swayc->x;
			double view_sy = oy - swayc->y;
			int width = swayc->sway_view->surface->current->width;
			int height = swayc->sway_view->surface->current->height;

			switch (sview->type) {
				case SWAY_WL_SHELL_VIEW:
					break;
				case SWAY_XDG_SHELL_V6_VIEW:
					// the top left corner of the sway container is the
					// coordinate of the top left corner of the window geometry
					view_sx += sview->wlr_xdg_surface_v6->geometry.x;
					view_sy += sview->wlr_xdg_surface_v6->geometry.y;

					// check for popups
					double popup_sx, popup_sy;
					struct wlr_xdg_surface_v6 *popup =
						wlr_xdg_surface_v6_popup_at(sview->wlr_xdg_surface_v6,
								view_sx, view_sy, &popup_sx, &popup_sy);

					if (popup) {
						*sx = view_sx - popup_sx;
						*sy = view_sy - popup_sy;
						*surface = popup->surface;
						return swayc;
					}
					break;
				case SWAY_XWAYLAND_VIEW:
					break;
				default:
					break;
			}

			// check for subsurfaces
			double sub_x, sub_y;
			struct wlr_subsurface *subsurface =
				wlr_surface_subsurface_at(sview->surface,
						view_sx, view_sy, &sub_x, &sub_y);
			if (subsurface) {
				*sx = view_sx - sub_x;
				*sy = view_sy - sub_y;
				*surface = subsurface->surface;
				return swayc;
			}

			if (view_sx > 0 && view_sx < width &&
					view_sy > 0 && view_sy < height &&
					pixman_region32_contains_point(
						&sview->surface->current->input,
						view_sx, view_sy, NULL)) {
				*sx = view_sx;
				*sy = view_sy;
				*surface = swayc->sway_view->surface;
				return swayc;
			}
		} else {
			list_cat(queue, swayc->children);
		}
	}

	return NULL;
}

void sway_container_for_each_bfs(struct sway_container *con,
		void (*f)(struct sway_container *con, void *data), void *data) {
	list_t *queue = get_bfs_queue();
	if (!queue) {
		return;
	}

	if (queue == NULL) {
		wlr_log(L_ERROR, "could not allocate list");
		return;
	}

	list_add(queue, con);

	struct sway_container *current = NULL;
	while (queue->length) {
		current = queue->items[0];
		list_del(queue, 0);
		f(current, data);
		// TODO floating containers
		list_cat(queue, current->children);
	}
}
