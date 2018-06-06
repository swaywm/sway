#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
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
#include "stringop.h"

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

static void container_update_textures_recursive(struct sway_container *con) {
	container_update_title_textures(con);

	if (con->type == C_VIEW) {
		view_update_marks_textures(con->sway_view);
	} else {
		for (int i = 0; i < con->children->length; ++i) {
			struct sway_container *child = con->children->items[i];
			container_update_textures_recursive(child);
		}
	}
}

static void handle_reparent(struct wl_listener *listener,
		void *data) {
	struct sway_container *container =
		wl_container_of(listener, container, reparent);
	struct sway_container *old_parent = data;

	struct sway_container *old_output = old_parent;
	if (old_output != NULL && old_output->type != C_OUTPUT) {
		old_output = container_parent(old_output, C_OUTPUT);
	}

	struct sway_container *new_output = container->parent;
	if (new_output != NULL && new_output->type != C_OUTPUT) {
		new_output = container_parent(new_output, C_OUTPUT);
	}

	if (old_output && new_output) {
		float old_scale = old_output->sway_output->wlr_output->scale;
		float new_scale = new_output->sway_output->wlr_output->scale;
		if (old_scale != new_scale) {
			container_update_textures_recursive(container);
		}
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

	wl_signal_add(&c->events.reparent, &c->reparent);
	c->reparent.notify = handle_reparent;

	return c;
}

static void _container_destroy(struct sway_container *cont) {
	if (cont == NULL) {
		return;
	}

	wl_signal_emit(&cont->events.destroy, cont);

	struct sway_container *parent = cont->parent;
	if (cont->children != NULL && cont->children->length) {
		// remove children until there are no more, container_destroy calls
		// container_remove_child, which removes child from this container
		while (cont->children != NULL && cont->children->length > 0) {
			struct sway_container *child = cont->children->items[0];
			ipc_event_window(child, "close");
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

	wlr_texture_destroy(cont->title_focused);
	wlr_texture_destroy(cont->title_focused_inactive);
	wlr_texture_destroy(cont->title_unfocused);
	wlr_texture_destroy(cont->title_urgent);

	list_free(cont->children);
	cont->children = NULL;
	free(cont);
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

	wlr_log(L_DEBUG, "destroying workspace '%s'", workspace->name);
	ipc_event_window(workspace, "close");

	struct sway_container *parent = workspace->parent;
	if (!workspace_is_empty(workspace) && output) {
		// Move children to a different workspace on this output
		struct sway_container *new_workspace = NULL;
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
		struct sway_container *floating = workspace->sway_workspace->floating;
		for (int i = 0; i < floating->children->length; i++) {
			container_move_to(floating->children->items[i],
					new_workspace->sway_workspace->floating);
		}
	}

	struct sway_workspace *sway_workspace = workspace->sway_workspace;

	// This emits the destroy event and also destroys the swayc.
	_container_destroy(workspace);

	// Clean up the floating container
	sway_workspace->floating->parent = NULL;
	_container_destroy(sway_workspace->floating);

	free(sway_workspace);

	if (output) {
		output_damage_whole(output->sway_output);
	}

	return parent;
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
			// Move workspace from this output to another output
			struct sway_container *other_output =
				root_container.children->items[0];
			if (other_output == output) {
				other_output = root_container.children->items[1];
			}

			while (output->children->length) {
				struct sway_container *workspace = output->children->items[0];
				container_remove_child(workspace);
				if (workspace->children->length > 0) {
					container_add_child(other_output, workspace);
					ipc_event_workspace(workspace, NULL, "move");
				} else {
					container_workspace_destroy(workspace);
				}
			}
			container_sort_workspaces(other_output);
			arrange_output(other_output);
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

static void container_root_finish(struct sway_container *con) {
	wlr_log(L_ERROR, "TODO: destroy the root container");
}

bool container_reap_empty(struct sway_container *con) {
	if (con->layout == L_FLOATING) {
		// Don't reap the magical floating container that each workspace has
		return false;
	}
	switch (con->type) {
	case C_ROOT:
	case C_OUTPUT:
		// dont reap these
		break;
	case C_WORKSPACE:
		if (!workspace_is_visible(con) && workspace_is_empty(con)) {
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
					ipc_event_window(child, "close");
					container_remove_child(child);
					container_add_child(parent, child);
				}
			}
			ipc_event_window(con, "close");
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
	if (container->type == C_WORKSPACE) {
		return container_find(container->sway_workspace->floating, test, data);
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

static struct sway_container *container_at_view(struct sway_container *swayc,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (!sway_assert(swayc->type == C_VIEW, "Expected a view")) {
		return NULL;
	}
	struct sway_view *sview = swayc->sway_view;
	double view_sx = lx - sview->x;
	double view_sy = ly - sview->y;

	double _sx, _sy;
	struct wlr_surface *_surface = NULL;
	switch (sview->type) {
#ifdef HAVE_XWAYLAND
	case SWAY_VIEW_XWAYLAND:
		_surface = wlr_surface_surface_at(sview->surface,
				view_sx, view_sy, &_sx, &_sy);
		break;
#endif
	case SWAY_VIEW_XDG_SHELL_V6:
		// the top left corner of the sway container is the
		// coordinate of the top left corner of the window geometry
		view_sx += sview->wlr_xdg_surface_v6->geometry.x;
		view_sy += sview->wlr_xdg_surface_v6->geometry.y;

		_surface = wlr_xdg_surface_v6_surface_at(
				sview->wlr_xdg_surface_v6,
				view_sx, view_sy, &_sx, &_sy);
		break;
	case SWAY_VIEW_XDG_SHELL:
		// the top left corner of the sway container is the
		// coordinate of the top left corner of the window geometry
		view_sx += sview->wlr_xdg_surface->geometry.x;
		view_sy += sview->wlr_xdg_surface->geometry.y;

		_surface = wlr_xdg_surface_surface_at(
				sview->wlr_xdg_surface,
				view_sx, view_sy, &_sx, &_sy);
		break;
	}
	if (_surface) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
	}
	return swayc;
}

/**
 * container_at for a container with layout L_TABBED.
 */
static struct sway_container *container_at_tabbed(struct sway_container *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (ly < parent->y || ly > parent->y + parent->height) {
		return NULL;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	// Tab titles
	int title_height = container_titlebar_height();
	if (ly < parent->y + title_height) {
		int tab_width = parent->width / parent->children->length;
		int child_index = (lx - parent->x) / tab_width;
		if (child_index >= parent->children->length) {
			child_index = parent->children->length - 1;
		}
		struct sway_container *child = parent->children->items[child_index];
		return seat_get_focus_inactive(seat, child);
	}

	// Surfaces
	struct sway_container *current = seat_get_active_child(seat, parent);

	return container_at(current, lx, ly, surface, sx, sy);
}

/**
 * container_at for a container with layout L_STACKED.
 */
static struct sway_container *container_at_stacked(
		struct sway_container *parent, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (ly < parent->y || ly > parent->y + parent->height) {
		return NULL;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	// Title bars
	int title_height = container_titlebar_height();
	int child_index = (ly - parent->y) / title_height;
	if (child_index < parent->children->length) {
		struct sway_container *child = parent->children->items[child_index];
		return seat_get_focus_inactive(seat, child);
	}

	// Surfaces
	struct sway_container *current = seat_get_active_child(seat, parent);

	return container_at(current, lx, ly, surface, sx, sy);
}

/**
 * container_at for a container with layout L_HORIZ or L_VERT.
 */
static struct sway_container *container_at_linear(struct sway_container *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		struct wlr_box box = {
			.x = child->x,
			.y = child->y,
			.width = child->width,
			.height = child->height,
		};
		if (wlr_box_contains_point(&box, lx, ly)) {
			return container_at(child, lx, ly, surface, sx, sy);
		}
	}
	return NULL;
}

struct sway_container *container_at(struct sway_container *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (!sway_assert(parent->type >= C_WORKSPACE,
				"Expected workspace or deeper")) {
		return NULL;
	}
	if (parent->type == C_VIEW) {
		return container_at_view(parent, lx, ly, surface, sx, sy);
	}
	if (!parent->children->length) {
		return NULL;
	}

	switch (parent->layout) {
	case L_HORIZ:
	case L_VERT:
		return container_at_linear(parent, lx, ly, surface, sx, sy);
	case L_TABBED:
		return container_at_tabbed(parent, lx, ly, surface, sx, sy);
	case L_STACKED:
		return container_at_stacked(parent, lx, ly, surface, sx, sy);
	case L_FLOATING:
		sway_assert(false, "Didn't expect to see floating here");
		return NULL;
	case L_NONE:
		return NULL;
	}

	return NULL;
}

struct sway_container *floating_container_at(double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		for (int j = 0; j < output->children->length; ++j) {
			struct sway_container *workspace = output->children->items[j];
			struct sway_workspace *ws = workspace->sway_workspace;
			if (!workspace_is_visible(workspace)) {
				continue;
			}
			for (int k = 0; k < ws->floating->children->length; ++k) {
				struct sway_container *floater =
					ws->floating->children->items[k];
				struct wlr_box box = {
					.x = floater->x,
					.y = floater->y,
					.width = floater->width,
					.height = floater->height,
				};
				if (wlr_box_contains_point(&box, lx, ly)) {
					return container_at(floater, lx, ly, surface, sx, sy);
				}
			}
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

bool container_has_ancestor(struct sway_container *descendant,
		struct sway_container *ancestor) {
	while (descendant->type != C_ROOT) {
		descendant = descendant->parent;
		if (descendant == ancestor) {
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
	if (con == NULL || con->type == C_VIEW) {
		return false;
	}
	return container_find(con, find_child_func, child);
}

int container_count_descendants_of_type(struct sway_container *con,
		enum sway_container_type type) {
	int children = 0;
	if (con->type == type) {
		children++;
	}
	if (con->children) {
		for (int i = 0; i < con->children->length; i++) {
			struct sway_container *child = con->children->items[i];
			children += container_count_descendants_of_type(child, type);
		}
	}
	return children;
}

void container_damage_whole(struct sway_container *container) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_whole_container(cont->sway_output, container);
		}
	}
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
		*texture = NULL;
	}
	if (!con->formatted_title) {
		return;
	}

	double scale = output->sway_output->wlr_output->scale;
	int width = 0;
	int height = config->font_height * scale;

	cairo_t *c = cairo_create(NULL);
	get_text_size(c, config->font, &width, NULL, scale, config->pango_markup,
			"%s", con->formatted_title);
	cairo_destroy(c);

	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_source_rgba(cairo, class->background[0], class->background[1],
			class->background[2], class->background[3]);
	cairo_paint(cairo);
	PangoContext *pango = pango_cairo_create_context(cairo);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_source_rgba(cairo, class->text[0], class->text[1],
			class->text[2], class->text[3]);
	cairo_move_to(cairo, 0, 0);

	pango_printf(cairo, config->font, scale, config->pango_markup,
			"%s", con->formatted_title);

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
	container_damage_whole(container);
}

void container_calculate_title_height(struct sway_container *container) {
	if (!container->formatted_title) {
		container->title_height = 0;
		return;
	}
	cairo_t *cairo = cairo_create(NULL);
	int height;
	get_text_size(cairo, config->font, NULL, &height, 1, config->pango_markup,
			"%s", container->formatted_title);
	cairo_destroy(cairo);
	container->title_height = height;
}

/**
 * Calculate and return the length of the tree representation.
 * An example tree representation is: V[Terminal, Firefox]
 * If buffer is not NULL, also populate the buffer with the representation.
 */
static size_t get_tree_representation(struct sway_container *parent, char *buffer) {
	size_t len = 2;
	switch (parent->layout) {
	case L_VERT:
		lenient_strcat(buffer, "V[");
		break;
	case L_HORIZ:
		lenient_strcat(buffer, "H[");
		break;
	case L_TABBED:
		lenient_strcat(buffer, "T[");
		break;
	case L_STACKED:
		lenient_strcat(buffer, "S[");
		break;
	case L_FLOATING:
		lenient_strcat(buffer, "F[");
		break;
	case L_NONE:
		lenient_strcat(buffer, "D[");
		break;
	}
	for (int i = 0; i < parent->children->length; ++i) {
		if (i != 0) {
			++len;
			lenient_strcat(buffer, " ");
		}
		struct sway_container *child = parent->children->items[i];
		const char *identifier = NULL;
		if (child->type == C_VIEW) {
			identifier = view_get_class(child->sway_view);
			if (!identifier) {
				identifier = view_get_app_id(child->sway_view);
			}
		} else {
			identifier = child->formatted_title;
		}
		if (identifier) {
			len += strlen(identifier);
			lenient_strcat(buffer, identifier);
		} else {
			len += 6;
			lenient_strcat(buffer, "(null)");
		}
	}
	++len;
	lenient_strcat(buffer, "]");
	return len;
}

void container_notify_subtree_changed(struct sway_container *container) {
	if (!container || container->type < C_WORKSPACE) {
		return;
	}
	free(container->formatted_title);
	container->formatted_title = NULL;

	size_t len = get_tree_representation(container, NULL);
	char *buffer = calloc(len + 1, sizeof(char));
	if (!sway_assert(buffer, "Unable to allocate title string")) {
		return;
	}
	get_tree_representation(container, buffer);

	container->formatted_title = buffer;
	if (container->type != C_WORKSPACE) {
		container_calculate_title_height(container);
		container_update_title_textures(container);
		container_notify_subtree_changed(container->parent);
	}
}

size_t container_titlebar_height() {
	return config->font_height + TITLEBAR_V_PADDING * 2;
}

void container_set_floating(struct sway_container *container, bool enable) {
	if (container_is_floating(container) == enable) {
		return;
	}

	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	container_damage_whole(container);

	if (enable) {
		container_remove_child(container);
		container_add_child(workspace->sway_workspace->floating, container);
		if (container->type == C_VIEW) {
			view_autoconfigure(container->sway_view);
		}
		seat_set_focus(seat, seat_get_focus_inactive(seat, container));
		container_reap_empty_recursive(workspace);
	} else {
		// Returning to tiled
		container_remove_child(container);
		container_add_child(workspace, container);
		container->width = container->parent->width;
		container->height = container->parent->height;
		container->is_sticky = false;
		container_reap_empty_recursive(workspace->sway_workspace->floating);
	}
	arrange_workspace(workspace);
	container_damage_whole(container);
}

void container_set_geometry_from_floating_view(struct sway_container *con) {
	if (!sway_assert(con->type == C_VIEW, "Expected a view")) {
		return;
	}
	if (!sway_assert(container_is_floating(con),
				"Expected a floating view")) {
		return;
	}
	struct sway_view *view = con->sway_view;
	size_t border_width = view->border_thickness * (view->border != B_NONE);
	size_t top =
		view->border == B_NORMAL ? container_titlebar_height() : border_width;

	con->x = view->x - border_width;
	con->y = view->y - top;
	con->width = view->width + border_width * 2;
	con->height = top + view->height + border_width;
}

bool container_is_floating(struct sway_container *container) {
	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	if (!workspace) {
		return false;
	}
	return container->parent == workspace->sway_workspace->floating;
}
