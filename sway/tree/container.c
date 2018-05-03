#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_wl_shell.h>
#include "cairo.h"
#include "pango.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
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

const char *container_type_to_str(enum sway_container_type type) {
	switch (type) {
	case C_ROOT:
		return "C_ROOT";
	case C_OUTPUT:
		return "C_OUTPUT";
	case C_WORKSPACE:
		return "C_WORKSPACE";
	case C_CONTAINER:
		return "C_CONTAINER";
	case C_VIEW:
		return "C_VIEW";
	default:
		return "C_UNKNOWN";
	}
}

void container_create_notify(struct sway_container *container) {
	// TODO send ipc event type based on the container type
	wl_signal_emit(&root_container.sway_root->events.new_container, container);

	if (container->type == C_VIEW || container->type == C_CONTAINER) {
		ipc_event_window(container, "new");
	}
}

static void container_close_notify(struct sway_container *container) {
	if (container == NULL) {
		return;
	}
	// TODO send ipc event type based on the container type
	if (container->type == C_VIEW || container->type == C_WORKSPACE) {
		ipc_event_window(container, "close");
	}
}

struct sway_container *container_create(enum sway_container_type type) {
	// next id starts at 1 because 0 is assigned to root_container in layout.c
	static size_t next_id = 1;
	struct sway_container *c = calloc(1, sizeof(struct sway_container));
	if (!c) {
		return NULL;
	}
	c->id = next_id++;
	c->layout = L_NONE;
	c->type = type;
	c->alpha = 1.0f;

	if (type != C_VIEW) {
		c->children = create_list();
	}

	wl_signal_init(&c->events.destroy);
	wl_signal_init(&c->events.reparent);

	return c;
}

static void _container_destroy(struct sway_container *cont) {
	if (cont == NULL) {
		return;
	}

	wl_signal_emit(&cont->events.destroy, cont);
	container_close_notify(cont);

	struct sway_container *parent = cont->parent;
	if (cont->children != NULL && cont->children->length) {
		// remove children until there are no more, container_destroy calls
		// container_remove_child, which removes child from this container
		while (cont->children != NULL && cont->children->length > 0) {
			struct sway_container *child = cont->children->items[0];
			container_remove_child(child);
			_container_destroy(child);
		}
	}
	if (cont->marks) {
		list_foreach(cont->marks, free);
		list_free(cont->marks);
	}
	if (parent) {
		parent = container_remove_child(cont);
	}
	if (cont->name) {
		free(cont->name);
	}
	if (cont->title_focused) {
		// If one is set then all of these are set
		wlr_texture_destroy(cont->title_focused);
		wlr_texture_destroy(cont->title_focused_inactive);
		wlr_texture_destroy(cont->title_unfocused);
		wlr_texture_destroy(cont->title_urgent);
	}
	list_free(cont->children);
	cont->children = NULL;
	free(cont);
}

static struct sway_container *container_output_destroy(
		struct sway_container *output) {
	if (!sway_assert(output, "cannot destroy null output")) {
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
			arrange_output(root_container.children->items[p]);
		}
	}

	wl_list_remove(&output->sway_output->destroy.link);
	wl_list_remove(&output->sway_output->mode.link);
	wl_list_remove(&output->sway_output->transform.link);
	wl_list_remove(&output->sway_output->scale.link);

	wl_list_remove(&output->sway_output->damage_destroy.link);
	wl_list_remove(&output->sway_output->damage_frame.link);

	// clear the wlr_output reference to this container
	output->sway_output->wlr_output->data = NULL;

	wlr_log(L_DEBUG, "OUTPUT: Destroying output '%s'", output->name);
	_container_destroy(output);
	return &root_container;
}

static struct sway_container *container_workspace_destroy(
		struct sway_container *workspace) {
	if (!sway_assert(workspace, "cannot destroy null workspace")) {
		return NULL;
	}

	// Do not destroy this if it's the last workspace on this output
	struct sway_container *output = container_parent(workspace, C_OUTPUT);
	if (output && output->children->length == 1) {
		return NULL;
	}

	struct sway_container *parent = workspace->parent;
	if (workspace->children->length == 0) {
		// destroy the WS if there are no children (TODO check for floating)
		wlr_log(L_DEBUG, "destroying workspace '%s'", workspace->name);
		ipc_event_workspace(workspace, NULL, "empty");
	} else {
		// Move children to a different workspace on this output
		struct sway_container *new_workspace = NULL;
		// TODO move floating
		for (int i = 0; i < output->children->length; i++) {
			if (output->children->items[i] != workspace) {
				new_workspace = output->children->items[i];
				break;
			}
		}

		wlr_log(L_DEBUG, "moving children to different workspace '%s' -> '%s'",
			workspace->name, new_workspace->name);
		for (int i = 0; i < workspace->children->length; i++) {
			container_move_to(workspace->children->items[i], new_workspace);
		}
	}

	free(workspace->sway_workspace);
	_container_destroy(workspace);

	output_damage_whole(output->sway_output);

	return parent;
}

static void container_root_finish(struct sway_container *con) {
	wlr_log(L_ERROR, "TODO: destroy the root container");
}

bool container_reap_empty(struct sway_container *con) {
	switch (con->type) {
	case C_ROOT:
	case C_OUTPUT:
		// dont reap these
		break;
	case C_WORKSPACE:
		if (!workspace_is_visible(con) && con->children->length == 0) {
			wlr_log(L_DEBUG, "Destroying workspace via reaper");
			container_workspace_destroy(con);
			return true;
		}
		break;
	case C_CONTAINER:
		if (con->children->length == 0) {
			_container_destroy(con);
			return true;
		}
	case C_VIEW:
		break;
	case C_TYPES:
		sway_assert(false, "container_reap_empty called on an invalid "
			"container");
		break;
	}

	return false;
}

struct sway_container *container_reap_empty_recursive(
		struct sway_container *con) {
	while (con) {
		struct sway_container *next = con->parent;
		if (!container_reap_empty(con)) {
			break;
		}
		con = next;
	}
	return con;
}

struct sway_container *container_flatten(struct sway_container *container) {
	while (container->type == C_CONTAINER && container->children->length == 1) {
		struct sway_container *child = container->children->items[0];
		struct sway_container *parent = container->parent;
		container_replace_child(container, child);
		container_destroy(container);
		container = parent;
	}
	return container;
}

struct sway_container *container_destroy(struct sway_container *con) {
	if (con == NULL) {
		return NULL;
	}

	struct sway_container *parent = con->parent;

	switch (con->type) {
		case C_ROOT:
			container_root_finish(con);
			break;
		case C_OUTPUT:
			// dont try to reap the root after this
			container_output_destroy(con);
			break;
		case C_WORKSPACE:
			// dont try to reap the output after this
			container_workspace_destroy(con);
			break;
		case C_CONTAINER:
			if (con->children->length) {
				for (int i = 0; i < con->children->length; ++i) {
					struct sway_container *child = con->children->items[0];
					container_remove_child(child);
					container_add_child(parent, child);
				}
			}
			_container_destroy(con);
			break;
		case C_VIEW:
			_container_destroy(con);
			break;
		case C_TYPES:
			wlr_log(L_ERROR, "container_destroy called on an invalid "
				"container");
			break;
	}

	return container_reap_empty_recursive(parent);
}

static void container_close_func(struct sway_container *container, void *data) {
	if (container->type == C_VIEW) {
		view_close(container->sway_view);
	}
}

struct sway_container *container_close(struct sway_container *con) {
	if (!sway_assert(con != NULL,
			"container_close called with a NULL container")) {
		return NULL;
	}

	struct sway_container *parent = con->parent;

	if (con->type == C_VIEW) {
		view_close(con->sway_view);
	} else {
		container_for_each_descendant_dfs(con, container_close_func, NULL);
	}

	return parent;
}

struct sway_container *container_view_create(struct sway_container *sibling,
		struct sway_view *sway_view) {
	if (!sway_assert(sibling,
			"container_view_create called with NULL sibling/parent")) {
		return NULL;
	}
	const char *title = view_get_title(sway_view);
	struct sway_container *swayc = container_create(C_VIEW);
	wlr_log(L_DEBUG, "Adding new view %p:%s to container %p %d %s",
		swayc, title, sibling, sibling ? sibling->type : 0, sibling->name);
	// Setup values
	swayc->sway_view = sway_view;
	container_update_title(swayc, title ? title : "");
	swayc->width = 0;
	swayc->height = 0;

	if (sibling->type == C_WORKSPACE) {
		// Case of focused workspace, just create as child of it
		container_add_child(sibling, swayc);
	} else {
		// Regular case, create as sibling of current container
		container_add_sibling(sibling, swayc);
	}
	container_create_notify(swayc);
	return swayc;
}

void container_descendants(struct sway_container *root,
		enum sway_container_type type,
		void (*func)(struct sway_container *item, void *data), void *data) {
	if (!root->children || !root->children->length) {
		return;
	}
	for (int i = 0; i < root->children->length; ++i) {
		struct sway_container *item = root->children->items[i];
		if (item->type == type) {
			func(item, data);
		}
		container_descendants(item, type, func, data);
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

struct sway_container *container_at(struct sway_container *parent,
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
			double view_sx = ox - sview->x;
			double view_sy = oy - sview->y;

			double _sx, _sy;
			struct wlr_surface *_surface;
			switch (sview->type) {
			case SWAY_VIEW_XWAYLAND:
				_surface = wlr_surface_surface_at(sview->surface,
					view_sx, view_sy, &_sx, &_sy);
				break;
			case SWAY_VIEW_WL_SHELL:
				_surface = wlr_wl_shell_surface_surface_at(
					sview->wlr_wl_shell_surface,
					view_sx, view_sy, &_sx, &_sy);
				break;
			case SWAY_VIEW_XDG_SHELL_V6:
				// the top left corner of the sway container is the
				// coordinate of the top left corner of the window geometry
				view_sx += sview->wlr_xdg_surface_v6->geometry.x;
				view_sy += sview->wlr_xdg_surface_v6->geometry.y;

				_surface = wlr_xdg_surface_v6_surface_at(
					sview->wlr_xdg_surface_v6,
					view_sx, view_sy, &_sx, &_sy);
				break;
			}
			if (_surface) {
				*sx = _sx;
				*sy = _sy;
				*surface = _surface;
				return swayc;
			}
			// Check the view's decorations
			struct wlr_box swayc_box = {
				.x = swayc->x,
				.y = swayc->y,
				.width = swayc->width,
				.height = swayc->height,
			};
			if (wlr_box_contains_point(&swayc_box, ox, oy)) {
				return swayc;
			}
		} else {
			list_cat(queue, swayc->children);
		}
	}

	return NULL;
}

void container_for_each_descendant_dfs(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data),
		void *data) {
	if (container) {
		if (container->children)  {
			for (int i = 0; i < container->children->length; ++i) {
				struct sway_container *child =
					container->children->items[i];
				container_for_each_descendant_dfs(child, f, data);
			}
		}
		f(container, data);
	}
}

void container_for_each_descendant_bfs(struct sway_container *con,
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

bool container_has_anscestor(struct sway_container *descendant,
		struct sway_container *anscestor) {
	while (descendant->type != C_ROOT) {
		descendant = descendant->parent;
		if (descendant == anscestor) {
			return true;
		}
	}
	return false;
}

static bool find_child_func(struct sway_container *con, void *data) {
	struct sway_container *child = data;
	return con == child;
}

bool container_has_child(struct sway_container *con,
		struct sway_container *child) {
	if (con == NULL || con->type == C_VIEW || con->children->length == 0) {
		return false;
	}
	return container_find(con, find_child_func, child);
}

void container_damage_whole(struct sway_container *con) {
	struct sway_container *output = con;
	if (output->type != C_OUTPUT) {
		output = container_parent(output, C_OUTPUT);
	}
	output_damage_whole_container(output->sway_output, con);
}

static void update_title_texture(struct sway_container *con,
		struct wlr_texture **texture, struct border_colors *class) {
	if (!sway_assert(con->type == C_CONTAINER || con->type == C_VIEW,
			"Unexpected type %s", container_type_to_str(con->type))) {
		return;
	}
	if (!con->width) {
		return;
	}
	struct sway_container *output = container_parent(con, C_OUTPUT);
	if (!output) {
		return;
	}
	if (*texture) {
		wlr_texture_destroy(*texture);
	}
	if (!con->name) {
		return;
	}

	int width = con->width * output->sway_output->wlr_output->scale;
	int height = config->font_height * output->sway_output->wlr_output->scale;

	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	PangoContext *pango = pango_cairo_create_context(cairo);
	cairo_set_source_u32(cairo, class->text);
	cairo_move_to(cairo, 0, 0);

	pango_printf(cairo, config->font, output->sway_output->wlr_output->scale,
			false, "%s", con->name);

	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
			output->sway_output->wlr_output->backend);
	*texture = wlr_texture_from_pixels(
			renderer, WL_SHM_FORMAT_ARGB8888, stride, width, height, data);
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
}

void container_update_title_textures(struct sway_container *container) {
	update_title_texture(container, &container->title_focused,
			&config->border_colors.focused);
	update_title_texture(container, &container->title_focused_inactive,
			&config->border_colors.focused_inactive);
	update_title_texture(container, &container->title_unfocused,
			&config->border_colors.unfocused);
	update_title_texture(container, &container->title_urgent,
			&config->border_colors.urgent);
}

void container_calculate_title_height(struct sway_container *container) {
	if (!container->name) {
		container->title_height = 0;
		return;
	}
	cairo_t *cairo = cairo_create(NULL);
	int height;
	get_text_size(cairo, config->font, NULL, &height, 1, false,
			"%s", container->name);
	cairo_destroy(cairo);
	container->title_height = height;
}

static void container_notify_child_title_changed(
		struct sway_container *container) {
	if (!container || container->type != C_CONTAINER) {
		return;
	}
	if (container->layout != L_TABBED && container->layout != L_STACKED) {
		return;
	}
	if (container->name) {
		free(container->name);
	}
	// TODO: iterate children and concatenate their titles
	container->name = strdup("");
	container_calculate_title_height(container);
	container_update_title_textures(container);
	container_notify_child_title_changed(container->parent);
}

void container_update_title(struct sway_container *container,
		const char *new_title) {
	if (container->name && strcmp(container->name, new_title) == 0) {
		return;
	}
	if (container->name) {
		free(container->name);
	}
	container->name = strdup(new_title);
	container_calculate_title_height(container);
	container_update_title_textures(container);
	container_notify_child_title_changed(container->parent);

	size_t prev_max_height = config->font_height;
	config_find_font_height(false);
	if (config->font_height != prev_max_height) {
		arrange_root();
	}
}
