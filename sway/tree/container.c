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
#include "sway/desktop.h"
#include "sway/desktop/transaction.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "stringop.h"

struct sway_container *container_create(struct sway_view *view) {
	struct sway_container *c = calloc(1, sizeof(struct sway_container));
	if (!c) {
		wlr_log(WLR_ERROR, "Unable to allocate sway_container");
		return NULL;
	}
	node_init(&c->node, N_CONTAINER, c);
	c->layout = L_NONE;
	c->view = view;
	c->alpha = 1.0f;

	if (!view) {
		c->children = create_list();
		c->current.children = create_list();
	}
	c->outputs = create_list();

	wl_signal_init(&c->events.destroy);
	wl_signal_emit(&root->events.new_node, &c->node);

	return c;
}

void container_destroy(struct sway_container *con) {
	if (!sway_assert(con->node.destroying,
				"Tried to free container which wasn't marked as destroying")) {
		return;
	}
	if (!sway_assert(con->node.ntxnrefs == 0, "Tried to free container "
				"which is still referenced by transactions")) {
		return;
	}
	free(con->title);
	free(con->formatted_title);
	wlr_texture_destroy(con->title_focused);
	wlr_texture_destroy(con->title_focused_inactive);
	wlr_texture_destroy(con->title_unfocused);
	wlr_texture_destroy(con->title_urgent);
	list_free(con->children);
	list_free(con->current.children);
	list_free(con->outputs);

	if (con->view) {
		struct sway_view *view = con->view;
		view->container = NULL;
		free(view->title_format);
		view->title_format = NULL;

		if (view->destroying) {
			view_destroy(con->view);
		}
	}

	free(con);
}

void container_begin_destroy(struct sway_container *con) {
	if (con->view) {
		ipc_event_window(con, "close");
	}
	// The workspace must have the fullscreen pointer cleared so that the
	// seat code can find an appropriate new focus.
	if (con->is_fullscreen && con->workspace) {
		con->workspace->fullscreen = NULL;
	}
	wl_signal_emit(&con->node.events.destroy, &con->node);

	container_end_mouse_operation(con);

	con->node.destroying = true;
	node_set_dirty(&con->node);

	if (con->scratchpad) {
		root_scratchpad_remove_container(con);
	}

	if (con->parent || con->workspace) {
		container_detach(con);
	}
}

void container_reap_empty(struct sway_container *con) {
	if (con->view) {
		return;
	}
	struct sway_workspace *ws = con->workspace;
	while (con) {
		if (con->children->length) {
			return;
		}
		struct sway_container *parent = con->parent;
		container_begin_destroy(con);
		con = parent;
	}
	workspace_consider_destroy(ws);
}

struct sway_container *container_flatten(struct sway_container *container) {
	if (container->view) {
		return NULL;
	}
	while (container && container->children->length == 1) {
		struct sway_container *child = container->children->items[0];
		struct sway_container *parent = container->parent;
		container_replace(container, child);
		container_begin_destroy(container);
		container = parent;
	}
	return container;
}

struct sway_container *container_find_child(struct sway_container *container,
		bool (*test)(struct sway_container *con, void *data), void *data) {
	if (!container->children) {
		return NULL;
	}
	for (int i = 0; i < container->children->length; ++i) {
		struct sway_container *child = container->children->items[i];
		if (test(child, data)) {
			return child;
		}
		struct sway_container *res = container_find_child(child, test, data);
		if (res) {
			return res;
		}
	}
	return NULL;
}

static void surface_at_view(struct sway_container *con, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (!sway_assert(con->view, "Expected a view")) {
		return;
	}
	struct sway_view *view = con->view;
	double view_sx = lx - view->x + view->geometry.x;
	double view_sy = ly - view->y + view->geometry.y;

	double _sx, _sy;
	struct wlr_surface *_surface = NULL;
	switch (view->type) {
#ifdef HAVE_XWAYLAND
	case SWAY_VIEW_XWAYLAND:
		_surface = wlr_surface_surface_at(view->surface,
				view_sx, view_sy, &_sx, &_sy);
		break;
#endif
	case SWAY_VIEW_XDG_SHELL_V6:
		_surface = wlr_xdg_surface_v6_surface_at(
				view->wlr_xdg_surface_v6,
				view_sx, view_sy, &_sx, &_sy);
		break;
	case SWAY_VIEW_XDG_SHELL:
		_surface = wlr_xdg_surface_surface_at(
				view->wlr_xdg_surface,
				view_sx, view_sy, &_sx, &_sy);
		break;
	}
	if (_surface) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
	}
}

/**
 * container_at for a container with layout L_TABBED.
 */
static struct sway_container *container_at_tabbed(struct sway_node *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	struct wlr_box box;
	node_get_box(parent, &box);
	if (ly < box.y || ly > box.y + box.height) {
		return NULL;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	list_t *children = node_get_children(parent);
	if (!children->length) {
		return NULL;
	}

	// Tab titles
	int title_height = container_titlebar_height();
	if (ly < box.y + title_height) {
		int tab_width = box.width / children->length;
		int child_index = (lx - box.x) / tab_width;
		if (child_index >= children->length) {
			child_index = children->length - 1;
		}
		struct sway_container *child = children->items[child_index];
		struct sway_node *node = seat_get_focus_inactive(seat, &child->node);
		return node->sway_container;
	}

	// Surfaces
	struct sway_node *current = seat_get_active_tiling_child(seat, parent);
	return current ? tiling_container_at(current, lx, ly, surface, sx, sy) : NULL;
}

/**
 * container_at for a container with layout L_STACKED.
 */
static struct sway_container *container_at_stacked(struct sway_node *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	struct wlr_box box;
	node_get_box(parent, &box);
	if (ly < box.y || ly > box.y + box.height) {
		return NULL;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	list_t *children = node_get_children(parent);

	// Title bars
	int title_height = container_titlebar_height();
	int child_index = (ly - box.y) / title_height;
	if (child_index < children->length) {
		struct sway_container *child = children->items[child_index];
		struct sway_node *node = seat_get_focus_inactive(seat, &child->node);
		return node->sway_container;
	}

	// Surfaces
	struct sway_node *current = seat_get_active_tiling_child(seat, parent);
	return current ? tiling_container_at(current, lx, ly, surface, sx, sy) : NULL;
}

/**
 * container_at for a container with layout L_HORIZ or L_VERT.
 */
static struct sway_container *container_at_linear(struct sway_node *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	list_t *children = node_get_children(parent);
	for (int i = 0; i < children->length; ++i) {
		struct sway_container *child = children->items[i];
		struct wlr_box box = {
			.x = child->x,
			.y = child->y,
			.width = child->width,
			.height = child->height,
		};
		if (wlr_box_contains_point(&box, lx, ly)) {
			return tiling_container_at(&child->node, lx, ly, surface, sx, sy);
		}
	}
	return NULL;
}

static struct sway_container *floating_container_at(double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		for (int j = 0; j < output->workspaces->length; ++j) {
			struct sway_workspace *ws = output->workspaces->items[j];
			if (!workspace_is_visible(ws)) {
				continue;
			}
			// Items at the end of the list are on top, so iterate the list in
			// reverse.
			for (int k = ws->floating->length - 1; k >= 0; --k) {
				struct sway_container *floater = ws->floating->items[k];
				struct wlr_box box = {
					.x = floater->x,
					.y = floater->y,
					.width = floater->width,
					.height = floater->height,
				};
				if (wlr_box_contains_point(&box, lx, ly)) {
					return tiling_container_at(&floater->node, lx, ly,
							surface, sx, sy);
				}
			}
		}
	}
	return NULL;
}

struct sway_container *tiling_container_at(struct sway_node *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (node_is_view(parent)) {
		surface_at_view(parent->sway_container, lx, ly, surface, sx, sy);
		return parent->sway_container;
	}
	if (!node_get_children(parent)) {
		return NULL;
	}
	switch (node_get_layout(parent)) {
	case L_HORIZ:
	case L_VERT:
		return container_at_linear(parent, lx, ly, surface, sx, sy);
	case L_TABBED:
		return container_at_tabbed(parent, lx, ly, surface, sx, sy);
	case L_STACKED:
		return container_at_stacked(parent, lx, ly, surface, sx, sy);
	case L_NONE:
		return NULL;
	}
	return NULL;
}

static bool surface_is_popup(struct wlr_surface *surface) {
	if (wlr_surface_is_xdg_surface(surface)) {
		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_from_wlr_surface(surface);
		while (xdg_surface) {
			if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
				return true;
			}
			xdg_surface = xdg_surface->toplevel->parent;
		}
		return false;
	}

	if (wlr_surface_is_xdg_surface_v6(surface)) {
		struct wlr_xdg_surface_v6 *xdg_surface_v6 =
			wlr_xdg_surface_v6_from_wlr_surface(surface);
		while (xdg_surface_v6) {
			if (xdg_surface_v6->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
				return true;
			}
			xdg_surface_v6 = xdg_surface_v6->toplevel->parent;
		}
		return false;
	}

	return false;
}

struct sway_container *container_at(struct sway_workspace *workspace,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	struct sway_container *c;
	// Focused view's popups
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focused_container(seat);
	bool is_floating = focus && container_is_floating_or_child(focus);
	// Focused view's popups
	if (focus && focus->view) {
		surface_at_view(focus, lx, ly, surface, sx, sy);
		if (*surface && surface_is_popup(*surface)) {
			return focus;
		}
		*surface = NULL;
	}
	// If focused is floating, focused view's non-popups
	if (focus && focus->view && is_floating) {
		surface_at_view(focus, lx, ly, surface, sx, sy);
		if (*surface) {
			return focus;
		}
		*surface = NULL;
	}
	// Floating (non-focused)
	if ((c = floating_container_at(lx, ly, surface, sx, sy))) {
		return c;
	}
	// If focused is tiling, focused view's non-popups
	if (focus && focus->view && !is_floating) {
		surface_at_view(focus, lx, ly, surface, sx, sy);
		if (*surface) {
			return focus;
		}
		*surface = NULL;
	}
	// Tiling (non-focused)
	if ((c = tiling_container_at(&workspace->node, lx, ly, surface, sx, sy))) {
		return c;
	}
	return NULL;
}

void container_for_each_child(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data),
		void *data) {
	if (container->children)  {
		for (int i = 0; i < container->children->length; ++i) {
			struct sway_container *child = container->children->items[i];
			f(child, data);
			container_for_each_child(child, f, data);
		}
	}
}

bool container_has_ancestor(struct sway_container *descendant,
		struct sway_container *ancestor) {
	while (descendant) {
		descendant = descendant->parent;
		if (descendant == ancestor) {
			return true;
		}
	}
	return false;
}

void container_damage_whole(struct sway_container *container) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_whole_container(output, container);
	}
}

/**
 * Return the output which will be used for scale purposes.
 * This is the most recently entered output.
 */
struct sway_output *container_get_effective_output(struct sway_container *con) {
	if (con->outputs->length == 0) {
		return NULL;
	}
	return con->outputs->items[con->outputs->length - 1];
}

static void update_title_texture(struct sway_container *con,
		struct wlr_texture **texture, struct border_colors *class) {
	struct sway_output *output = container_get_effective_output(con);
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

	double scale = output->wlr_output->scale;
	int width = 0;
	int height = con->title_height * scale;

	cairo_t *c = cairo_create(NULL);
	get_text_size(c, config->font, &width, NULL, NULL, scale,
			config->pango_markup, "%s", con->formatted_title);
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
			output->wlr_output->backend);
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
	int baseline;
	get_text_size(cairo, config->font, NULL, &height, &baseline, 1,
			config->pango_markup, "%s", container->formatted_title);
	cairo_destroy(cairo);
	container->title_height = height;
	container->title_baseline = baseline;
}

/**
 * Calculate and return the length of the tree representation.
 * An example tree representation is: V[Terminal, Firefox]
 * If buffer is not NULL, also populate the buffer with the representation.
 */
size_t container_build_representation(enum sway_container_layout layout,
		list_t *children, char *buffer) {
	size_t len = 2;
	switch (layout) {
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
	case L_NONE:
		lenient_strcat(buffer, "D[");
		break;
	}
	for (int i = 0; i < children->length; ++i) {
		if (i != 0) {
			++len;
			lenient_strcat(buffer, " ");
		}
		struct sway_container *child = children->items[i];
		const char *identifier = NULL;
		if (child->view) {
			identifier = view_get_class(child->view);
			if (!identifier) {
				identifier = view_get_app_id(child->view);
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

void container_update_representation(struct sway_container *con) {
	if (!con->view) {
		size_t len = container_build_representation(con->layout,
				con->children, NULL);
		free(con->formatted_title);
		con->formatted_title = calloc(len + 1, sizeof(char));
		if (!sway_assert(con->formatted_title,
					"Unable to allocate title string")) {
			return;
		}
		container_build_representation(con->layout, con->children,
				con->formatted_title);
		container_calculate_title_height(con);
		container_update_title_textures(con);
	}
	if (con->parent) {
		container_update_representation(con->parent);
	} else if (con->workspace) {
		workspace_update_representation(con->workspace);
	}
}

size_t container_titlebar_height() {
	return config->font_height + TITLEBAR_V_PADDING * 2;
}

void container_init_floating(struct sway_container *con) {
	struct sway_workspace *ws = con->workspace;
	int min_width, min_height;
	int max_width, max_height;

	if (config->floating_minimum_width == -1) { // no minimum
		min_width = 0;
	} else if (config->floating_minimum_width == 0) { // automatic
		min_width = 75;
	} else {
		min_width = config->floating_minimum_width;
	}

	if (config->floating_minimum_height == -1) { // no minimum
		min_height = 0;
	} else if (config->floating_minimum_height == 0) { // automatic
		min_height = 50;
	} else {
		min_height = config->floating_minimum_height;
	}

	if (config->floating_maximum_width == -1) { // no maximum
		max_width = INT_MAX;
	} else if (config->floating_maximum_width == 0) { // automatic
		max_width = ws->width * 0.6666;
	} else {
		max_width = config->floating_maximum_width;
	}

	if (config->floating_maximum_height == -1) { // no maximum
		max_height = INT_MAX;
	} else if (config->floating_maximum_height == 0) { // automatic
		max_height = ws->height * 0.6666;
	} else {
		max_height = config->floating_maximum_height;
	}

	if (!con->view) {
		con->width = max_width;
		con->height = max_height;
		con->x = ws->x + (ws->width - con->width) / 2;
		con->y = ws->y + (ws->height - con->height) / 2;
	} else {
		struct sway_view *view = con->view;
		view->width = fmax(min_width, fmin(view->natural_width, max_width));
		view->height = fmax(min_height, fmin(view->natural_height, max_height));
		view->x = ws->x + (ws->width - view->width) / 2;
		view->y = ws->y + (ws->height - view->height) / 2;

		// If the view's border is B_NONE then these properties are ignored.
		view->border_top = view->border_bottom = true;
		view->border_left = view->border_right = true;

		container_set_geometry_from_floating_view(con);
	}
}

void container_set_floating(struct sway_container *container, bool enable) {
	if (container_is_floating(container) == enable) {
		return;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_workspace *workspace = container->workspace;

	if (enable) {
		struct sway_container *old_parent = container->parent;
		container_detach(container);
		workspace_add_floating(workspace, container);
		container_init_floating(container);
		if (container->view) {
			view_set_tiled(container->view, false);
		}
		if (old_parent) {
			container_reap_empty(old_parent);
		}
	} else {
		// Returning to tiled
		if (container->scratchpad) {
			root_scratchpad_remove_container(container);
		}
		container_detach(container);
		struct sway_container *reference =
			seat_get_focus_inactive_tiling(seat, workspace);
		if (reference && reference->view) {
			reference = reference->parent;
		}
		if (reference) {
			container_add_child(reference, container);
			container->width = reference->width;
			container->height = reference->height;
		} else {
			workspace_add_tiling(workspace, container);
			container->width = workspace->width;
			container->height = workspace->height;
		}
		if (container->view) {
			view_set_tiled(container->view, true);
		}
		container->is_sticky = false;
	}

	container_end_mouse_operation(container);

	ipc_event_window(container, "floating");
}

void container_set_geometry_from_floating_view(struct sway_container *con) {
	if (!sway_assert(con->view, "Expected a view")) {
		return;
	}
	if (!sway_assert(container_is_floating(con), "Expected a floating view")) {
		return;
	}
	struct sway_view *view = con->view;
	size_t border_width = 0;
	size_t top = 0;

	if (!view->using_csd) {
		border_width = view->border_thickness * (view->border != B_NONE);
		top = view->border == B_NORMAL ?
			container_titlebar_height() : border_width;
	}

	con->x = view->x - border_width;
	con->y = view->y - top;
	con->width = view->width + border_width * 2;
	con->height = top + view->height + border_width;
	node_set_dirty(&con->node);
}

bool container_is_floating(struct sway_container *container) {
	return !container->parent && container->workspace &&
		list_find(container->workspace->floating, container) != -1;
}

void container_get_box(struct sway_container *container, struct wlr_box *box) {
	box->x = container->x;
	box->y = container->y;
	box->width = container->width;
	box->height = container->height;
}

/**
 * Translate the container's position as well as all children.
 */
void container_floating_translate(struct sway_container *con,
		double x_amount, double y_amount) {
	con->x += x_amount;
	con->y += y_amount;
	if (con->view) {
		con->view->x += x_amount;
		con->view->y += y_amount;
	} else {
		for (int i = 0; i < con->children->length; ++i) {
			struct sway_container *child = con->children->items[i];
			container_floating_translate(child, x_amount, y_amount);
		}
	}
	node_set_dirty(&con->node);
}

/**
 * Choose an output for the floating container's new position.
 *
 * If the center of the container intersects an output then we'll choose that
 * one, otherwise we'll choose whichever output is closest to the container's
 * center.
 */
struct sway_output *container_floating_find_output(struct sway_container *con) {
	double center_x = con->x + con->width / 2;
	double center_y = con->y + con->height / 2;
	struct sway_output *closest_output = NULL;
	double closest_distance = DBL_MAX;
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		struct wlr_box output_box;
		double closest_x, closest_y;
		output_get_box(output, &output_box);
		wlr_box_closest_point(&output_box, center_x, center_y,
				&closest_x, &closest_y);
		if (center_x == closest_x && center_y == closest_y) {
			// The center of the floating container is on this output
			return output;
		}
		double x_dist = closest_x - center_x;
		double y_dist = closest_y - center_y;
		double distance = x_dist * x_dist + y_dist * y_dist;
		if (distance < closest_distance) {
			closest_output = output;
			closest_distance = distance;
		}
	}
	return closest_output;
}

void container_floating_move_to(struct sway_container *con,
		double lx, double ly) {
	if (!sway_assert(container_is_floating(con),
			"Expected a floating container")) {
		return;
	}
	container_floating_translate(con, lx - con->x, ly - con->y);
	struct sway_workspace *old_workspace = con->workspace;
	struct sway_output *new_output = container_floating_find_output(con);
	if (!sway_assert(new_output, "Unable to find any output")) {
		return;
	}
	struct sway_workspace *new_workspace =
		output_get_active_workspace(new_output);
	if (old_workspace != new_workspace) {
		container_detach(con);
		workspace_add_floating(new_workspace, con);
		arrange_workspace(old_workspace);
		arrange_workspace(new_workspace);
		workspace_detect_urgent(old_workspace);
		workspace_detect_urgent(new_workspace);
	}
}

void container_floating_move_to_center(struct sway_container *con) {
	if (!sway_assert(container_is_floating(con),
			"Expected a floating container")) {
		return;
	}
	struct sway_workspace *ws = con->workspace;
	double new_lx = ws->x + (ws->width - con->width) / 2;
	double new_ly = ws->y + (ws->height - con->height) / 2;
	container_floating_translate(con, new_lx - con->x, new_ly - con->y);
}

static bool find_urgent_iterator(struct sway_container *con, void *data) {
	return con->view && view_is_urgent(con->view);
}

bool container_has_urgent_child(struct sway_container *container) {
	return container_find_child(container, find_urgent_iterator, NULL);
}

void container_end_mouse_operation(struct sway_container *container) {
	struct sway_seat *seat;
	wl_list_for_each(seat, &input_manager->seats, link) {
		if (seat->op_container == container) {
			seat->op_target_node = NULL; // ensure tiling move doesn't apply
			seat_end_mouse_operation(seat);
		}
		// If the user is doing a tiling drag over this container,
		// keep the operation active but unset the target container.
		if (seat->op_target_node == &container->node) {
			seat->op_target_node = NULL;
		}
	}
}

static void set_fullscreen_iterator(struct sway_container *con, void *data) {
	if (!con->view) {
		return;
	}
	if (con->view->impl->set_fullscreen) {
		bool *enable = data;
		con->view->impl->set_fullscreen(con->view, *enable);
	}
}

void container_set_fullscreen(struct sway_container *container, bool enable) {
	if (container->is_fullscreen == enable) {
		return;
	}

	struct sway_workspace *workspace = container->workspace;
	if (enable && workspace->fullscreen) {
		container_set_fullscreen(workspace->fullscreen, false);
	}

	set_fullscreen_iterator(container, &enable);
	container_for_each_child(container, set_fullscreen_iterator, &enable);

	container->is_fullscreen = enable;

	if (enable) {
		workspace->fullscreen = container;
		container->saved_x = container->x;
		container->saved_y = container->y;
		container->saved_width = container->width;
		container->saved_height = container->height;

		struct sway_seat *seat;
		struct sway_workspace *focus_ws;
		wl_list_for_each(seat, &input_manager->seats, link) {
			focus_ws = seat_get_focused_workspace(seat);
			if (focus_ws) {
				if (focus_ws == workspace) {
					seat_set_focus_container(seat, container);
				}
			}
		}
	} else {
		workspace->fullscreen = NULL;
		if (container_is_floating(container)) {
			container->x = container->saved_x;
			container->y = container->saved_y;
			container->width = container->saved_width;
			container->height = container->saved_height;
			struct sway_output *output =
				container_floating_find_output(container);
			if (workspace->output != output) {
				container_floating_move_to_center(container);
			}
		} else {
			container->width = container->saved_width;
			container->height = container->saved_height;
		}
	}

	container_end_mouse_operation(container);

	ipc_event_window(container, "fullscreen_mode");
}

bool container_is_floating_or_child(struct sway_container *container) {
	while (container->parent) {
		container = container->parent;
	}
	return container_is_floating(container);
}

bool container_is_fullscreen_or_child(struct sway_container *container) {
	do {
		if (container->is_fullscreen) {
			return true;
		}
		container = container->parent;
	} while (container);

	return false;
}

static void surface_send_enter_iterator(struct wlr_surface *surface,
		int x, int y, void *data) {
	struct wlr_output *wlr_output = data;
	wlr_surface_send_enter(surface, wlr_output);
}

static void surface_send_leave_iterator(struct wlr_surface *surface,
		int x, int y, void *data) {
	struct wlr_output *wlr_output = data;
	wlr_surface_send_leave(surface, wlr_output);
}

void container_discover_outputs(struct sway_container *con) {
	struct wlr_box con_box = {
		.x = con->current.con_x,
		.y = con->current.con_y,
		.width = con->current.con_width,
		.height = con->current.con_height,
	};
	struct sway_output *old_output = container_get_effective_output(con);

	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		struct wlr_box output_box;
		output_get_box(output, &output_box);
		struct wlr_box intersection;
		bool intersects =
			wlr_box_intersection(&con_box, &output_box, &intersection);
		int index = list_find(con->outputs, output);

		if (intersects && index == -1) {
			// Send enter
			wlr_log(WLR_DEBUG, "Container %p entered output %p", con, output);
			if (con->view) {
				view_for_each_surface(con->view,
						surface_send_enter_iterator, output->wlr_output);
			}
			list_add(con->outputs, output);
		} else if (!intersects && index != -1) {
			// Send leave
			wlr_log(WLR_DEBUG, "Container %p left output %p", con, output);
			if (con->view) {
				view_for_each_surface(con->view,
					surface_send_leave_iterator, output->wlr_output);
			}
			list_del(con->outputs, index);
		}
	}
	struct sway_output *new_output = container_get_effective_output(con);
	double old_scale = old_output ? old_output->wlr_output->scale : -1;
	double new_scale = new_output ? new_output->wlr_output->scale : -1;
	if (old_scale != new_scale) {
		container_update_title_textures(con);
		if (con->view) {
			view_update_marks_textures(con->view);
		}
	}
}

void container_remove_gaps(struct sway_container *c) {
	if (c->current_gaps == 0) {
		return;
	}

	c->width += c->current_gaps * 2;
	c->height += c->current_gaps * 2;
	c->x -= c->current_gaps;
	c->y -= c->current_gaps;
	c->current_gaps = 0;
}

void container_add_gaps(struct sway_container *c) {
	if (c->current_gaps > 0) {
		return;
	}
	// Linear containers don't have gaps because it'd create double gaps
	if (!c->view && c->layout != L_TABBED && c->layout != L_STACKED) {
		return;
	}
	// Children of tabbed/stacked containers re-use the gaps of the container
	enum sway_container_layout layout = container_parent_layout(c);
	if (layout == L_TABBED || layout == L_STACKED) {
		return;
	}

	struct sway_workspace *ws = c->workspace;

	c->current_gaps = ws->has_gaps ? ws->gaps_inner : config->gaps_inner;
	c->x += c->current_gaps;
	c->y += c->current_gaps;
	c->width -= 2 * c->current_gaps;
	c->height -= 2 * c->current_gaps;
}

enum sway_container_layout container_parent_layout(struct sway_container *con) {
	if (con->parent) {
		return con->parent->layout;
	}
	return con->workspace->layout;
}

enum sway_container_layout container_current_parent_layout(
		struct sway_container *con) {
	if (con->current.parent) {
		return con->current.parent->current.layout;
	}
	return con->current.workspace->current.layout;
}

list_t *container_get_siblings(const struct sway_container *container) {
	if (container->parent) {
		return container->parent->children;
	}
	if (!container->workspace) {
		return NULL;
	}
	if (list_find(container->workspace->tiling, container) != -1) {
		return container->workspace->tiling;
	}
	return container->workspace->floating;
}

int container_sibling_index(const struct sway_container *child) {
	return list_find(container_get_siblings(child), child);
}

list_t *container_get_current_siblings(struct sway_container *container) {
	if (container->current.parent) {
		return container->current.parent->current.children;
	}
	return container->current.workspace->current.tiling;
}

void container_handle_fullscreen_reparent(struct sway_container *con) {
	if (!con->is_fullscreen || !con->workspace ||
			con->workspace->fullscreen == con) {
		return;
	}
	if (con->workspace->fullscreen) {
		container_set_fullscreen(con->workspace->fullscreen, false);
	}
	con->workspace->fullscreen = con;

	arrange_workspace(con->workspace);
}

static void set_workspace(struct sway_container *container, void *data) {
	container->workspace = container->parent->workspace;
}

void container_insert_child(struct sway_container *parent,
		struct sway_container *child, int i) {
	if (child->workspace) {
		container_detach(child);
	}
	list_insert(parent->children, i, child);
	child->parent = parent;
	child->workspace = parent->workspace;
	container_for_each_child(child, set_workspace, NULL);
	container_handle_fullscreen_reparent(child);
	container_update_representation(parent);
}

void container_add_sibling(struct sway_container *fixed,
		struct sway_container *active, bool after) {
	if (active->workspace) {
		container_detach(active);
	}
	list_t *siblings = container_get_siblings(fixed);
	int index = list_find(siblings, fixed);
	list_insert(siblings, index + after, active);
	active->parent = fixed->parent;
	active->workspace = fixed->workspace;
	container_for_each_child(active, set_workspace, NULL);
	container_handle_fullscreen_reparent(active);
	container_update_representation(active);
}

void container_add_child(struct sway_container *parent,
		struct sway_container *child) {
	if (child->workspace) {
		container_detach(child);
	}
	list_add(parent->children, child);
	child->parent = parent;
	child->workspace = parent->workspace;
	container_for_each_child(child, set_workspace, NULL);
	container_handle_fullscreen_reparent(child);
	container_update_representation(parent);
	node_set_dirty(&child->node);
	node_set_dirty(&parent->node);
}

void container_detach(struct sway_container *child) {
	if (child->is_fullscreen) {
		child->workspace->fullscreen = NULL;
	}

	struct sway_container *old_parent = child->parent;
	struct sway_workspace *old_workspace = child->workspace;
	list_t *siblings = container_get_siblings(child);
	if (siblings) {
		int index = list_find(siblings, child);
		if (index != -1) {
			list_del(siblings, index);
		}
	}
	child->parent = NULL;
	child->workspace = NULL;
	container_for_each_child(child, set_workspace, NULL);

	if (old_parent) {
		container_update_representation(old_parent);
		node_set_dirty(&old_parent->node);
	} else if (old_workspace) {
		workspace_update_representation(old_workspace);
		node_set_dirty(&old_workspace->node);
	}
	node_set_dirty(&child->node);
}

void container_replace(struct sway_container *container,
		struct sway_container *replacement) {
	container_add_sibling(container, replacement, 1);
	container_detach(container);
}

struct sway_container *container_split(struct sway_container *child,
		enum sway_container_layout layout) {
	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	bool set_focus = (seat_get_focus(seat) == &child->node);

	struct sway_container *cont = container_create(NULL);
	cont->width = child->width;
	cont->height = child->height;
	cont->x = child->x;
	cont->y = child->y;
	cont->current_gaps = child->current_gaps;
	cont->layout = layout;

	container_replace(child, cont);
	container_add_child(cont, child);

	if (set_focus) {
		seat_set_focus_container(seat, cont);
		seat_set_focus_container(seat, child);
	}

	return cont;
}
