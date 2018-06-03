#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_layout.h>
#include "list.h"
#include "log.h"
#include "sway/criteria.h"
#include "sway/commands.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/input/seat.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/config.h"
#include "pango.h"
#include "stringop.h"

void view_init(struct sway_view *view, enum sway_view_type type,
		const struct sway_view_impl *impl) {
	view->type = type;
	view->impl = impl;
	view->executed_criteria = create_list();
	view->marks = create_list();
	view->instructions = create_list();
	wl_signal_init(&view->events.unmap);
}

void view_destroy(struct sway_view *view) {
	if (view == NULL) {
		return;
	}

	if (view->surface != NULL) {
		view_unmap(view);
	}

	if (!sway_assert(view->instructions->length == 0,
				"Tried to destroy view with pending instructions")) {
		return;
	}

	list_free(view->executed_criteria);

	for (int i = 0; i < view->marks->length; ++i) {
		free(view->marks->items[i]);
	}
	list_free(view->marks);

	list_free(view->instructions);

	wlr_texture_destroy(view->marks_focused);
	wlr_texture_destroy(view->marks_focused_inactive);
	wlr_texture_destroy(view->marks_unfocused);
	wlr_texture_destroy(view->marks_urgent);

	container_destroy(view->swayc);

	if (view->impl->destroy) {
		view->impl->destroy(view);
	} else {
		free(view);
	}
}

const char *view_get_title(struct sway_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_TITLE);
	}
	return NULL;
}

const char *view_get_app_id(struct sway_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_APP_ID);
	}
	return NULL;
}

const char *view_get_class(struct sway_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_CLASS);
	}
	return NULL;
}

const char *view_get_instance(struct sway_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_INSTANCE);
	}
	return NULL;
}

uint32_t view_get_x11_window_id(struct sway_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_X11_WINDOW_ID);
	}
	return 0;
}

const char *view_get_window_role(struct sway_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_WINDOW_ROLE);
	}
	return NULL;
}

uint32_t view_get_window_type(struct sway_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_WINDOW_TYPE);
	}
	return 0;
}

const char *view_get_shell(struct sway_view *view) {
	switch(view->type) {
	case SWAY_VIEW_XDG_SHELL_V6:
		return "xdg_shell_v6";
	case SWAY_VIEW_XDG_SHELL:
		return "xdg_shell";
	case SWAY_VIEW_XWAYLAND:
		return "xwayland";
	}
	return "unknown";
}

uint32_t view_configure(struct sway_view *view, double lx, double ly, int width,
		int height) {
	if (view->impl->configure) {
		return view->impl->configure(view, lx, ly, width, height);
	}
	return 0;
}

static void view_autoconfigure_floating(struct sway_view *view) {
	struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);
	int max_width = ws->width * 0.6666;
	int max_height = ws->height * 0.6666;
	int width =
		view->natural_width > max_width ? max_width : view->natural_width;
	int height =
		view->natural_height > max_height ? max_height : view->natural_height;
	int lx = ws->x + (ws->width - width) / 2;
	int ly = ws->y + (ws->height - height) / 2;

	// If the view's border is B_NONE then these properties are ignored.
	view->border_top = view->border_bottom = true;
	view->border_left = view->border_right = true;

	view_configure(view, lx, ly, width, height);
}

void view_autoconfigure(struct sway_view *view) {
	if (!sway_assert(view->swayc,
				"Called view_autoconfigure() on a view without a swayc")) {
		return;
	}

	struct sway_container *output = container_parent(view->swayc, C_OUTPUT);

	if (view->is_fullscreen) {
		view_configure(view, output->x, output->y, output->width, output->height);
		return;
	}

	if (container_is_floating(view->swayc)) {
		view_autoconfigure_floating(view);
		return;
	}

	struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);

	int other_views = 0;
	if (config->hide_edge_borders == E_SMART) {
		struct sway_container *con = view->swayc;
		while (con != output) {
			if (con->layout != L_TABBED && con->layout != L_STACKED) {
				other_views += con->children ? con->children->length - 1 : 0;
				if (other_views > 0) {
					break;
				}
			}
			con = con->parent;
		}
	}

	struct sway_container_state *state = &view->swayc->pending;

	state->border_top = state->border_bottom = true;
	state->border_left = state->border_right = true;
	if (config->hide_edge_borders == E_BOTH
			|| config->hide_edge_borders == E_VERTICAL
			|| (config->hide_edge_borders == E_SMART && !other_views)) {
		state->border_left = state->swayc_x != ws->x;
		int right_x = state->swayc_x + state->swayc_width;
		state->border_right = right_x != ws->x + ws->width;
	}
	if (config->hide_edge_borders == E_BOTH
			|| config->hide_edge_borders == E_HORIZONTAL
			|| (config->hide_edge_borders == E_SMART && !other_views)) {
		state->border_top = state->swayc_y != ws->y;
		int bottom_y = state->swayc_y + state->swayc_height;
		state->border_bottom = bottom_y != ws->y + ws->height;
	}

	double x, y, width, height;
	x = y = width = height = 0;
	double y_offset = 0;

	// In a tabbed or stacked container, the swayc's y is the top of the title
	// area. We have to offset the surface y by the height of the title bar, and
	// disable any top border because we'll always have the title bar.
	if (view->swayc->parent->pending.layout == L_TABBED) {
		y_offset = container_titlebar_height();
		state->border_top = false;
	} else if (view->swayc->parent->pending.layout == L_STACKED) {
		y_offset = container_titlebar_height()
			* view->swayc->parent->children->length;
		view->border_top = false;
	}

	switch (state->border) {
	case B_NONE:
		x = state->swayc_x;
		y = state->swayc_y + y_offset;
		width = state->swayc_width;
		height = state->swayc_height - y_offset;
		break;
	case B_PIXEL:
		x = state->swayc_x + state->border_thickness * state->border_left;
		y = state->swayc_y + state->border_thickness * state->border_top + y_offset;
		width = state->swayc_width
			- state->border_thickness * state->border_left
			- state->border_thickness * state->border_right;
		height = state->swayc_height - y_offset
			- state->border_thickness * state->border_top
			- state->border_thickness * state->border_bottom;
		break;
	case B_NORMAL:
		// Height is: 1px border + 3px pad + title height + 3px pad + 1px border
		x = state->swayc_x + state->border_thickness * state->border_left;
		width = state->swayc_width
			- state->border_thickness * state->border_left
			- state->border_thickness * state->border_right;
		if (y_offset) {
			y = state->swayc_y + y_offset;
			height = state->swayc_height - y_offset
				- state->border_thickness * state->border_bottom;
		} else {
			y = state->swayc_y + container_titlebar_height();
			height = state->swayc_height - container_titlebar_height()
				- state->border_thickness * state->border_bottom;
		}
		break;
	}

	state->view_x = x;
	state->view_y = y;
	state->view_width = width;
	state->view_height = height;
}

void view_set_activated(struct sway_view *view, bool activated) {
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
}

// Set fullscreen, but without IPC events or arranging windows.
void view_set_fullscreen_raw(struct sway_view *view, bool fullscreen) {
	if (view->is_fullscreen == fullscreen) {
		return;
	}

	struct sway_container *workspace =
		container_parent(view->swayc, C_WORKSPACE);

	if (view->impl->set_fullscreen) {
		view->impl->set_fullscreen(view, fullscreen);
	}

	view->is_fullscreen = fullscreen;

	if (fullscreen) {
		if (workspace->sway_workspace->fullscreen) {
			view_set_fullscreen(workspace->sway_workspace->fullscreen, false);
		}
		workspace->sway_workspace->fullscreen = view;
		view->saved_x = view->x;
		view->saved_y = view->y;
		view->saved_width = view->width;
		view->saved_height = view->height;
		view->swayc->saved_x = view->swayc->x;
		view->swayc->saved_y = view->swayc->y;
		view->swayc->saved_width = view->swayc->width;
		view->swayc->saved_height = view->swayc->height;

		struct sway_seat *seat;
		struct sway_container *focus, *focus_ws;
		wl_list_for_each(seat, &input_manager->seats, link) {
			focus = seat_get_focus(seat);
			if (focus) {
				focus_ws = focus;
				if (focus && focus_ws->type != C_WORKSPACE) {
					focus_ws = container_parent(focus_ws, C_WORKSPACE);
				}
				seat_set_focus(seat, view->swayc);
				if (focus_ws != workspace) {
					seat_set_focus(seat, focus);
				}
			}
		}
	} else {
		workspace->sway_workspace->fullscreen = NULL;
		if (container_is_floating(view->swayc)) {
			view_configure(view, view->saved_x, view->saved_y,
					view->saved_width, view->saved_height);
		} else {
		view->swayc->width = view->swayc->saved_width;
		view->swayc->height = view->swayc->saved_height;
			view_autoconfigure(view);
		}
	}
}

void view_set_fullscreen(struct sway_view *view, bool fullscreen) {
	if (view->is_fullscreen == fullscreen) {
		return;
	}

	view_set_fullscreen_raw(view, fullscreen);

	struct sway_container *workspace =
		container_parent(view->swayc, C_WORKSPACE);
	arrange_workspace(workspace);
	output_damage_whole(workspace->parent->sway_output);
	ipc_event_window(view->swayc, "fullscreen_mode");
}

void view_close(struct sway_view *view) {
	if (view->impl->close) {
		view->impl->close(view);
	}
}

void view_damage_from(struct sway_view *view) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_from_view(cont->sway_output, view);
		}
	}
}

static void view_get_layout_box(struct sway_view *view, struct wlr_box *box) {
	struct sway_container *output = container_parent(view->swayc, C_OUTPUT);

	box->x = output->x + view->swayc->x;
	box->y = output->y + view->swayc->y;
	box->width = view->width;
	box->height = view->height;
}

void view_for_each_surface(struct sway_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	if (view->impl->for_each_surface) {
		view->impl->for_each_surface(view, iterator, user_data);
	} else {
		wlr_surface_for_each_surface(view->surface, iterator, user_data);
	}
}

static void view_subsurface_create(struct sway_view *view,
	struct wlr_subsurface *subsurface);

static void view_init_subsurfaces(struct sway_view *view,
	struct wlr_surface *surface);

static void view_handle_surface_new_subsurface(struct wl_listener *listener,
		void *data) {
	struct sway_view *view =
		wl_container_of(listener, view, surface_new_subsurface);
	struct wlr_subsurface *subsurface = data;
	view_subsurface_create(view, subsurface);
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

static void view_handle_container_reparent(struct wl_listener *listener,
		void *data) {
	struct sway_view *view =
		wl_container_of(listener, view, container_reparent);
	struct sway_container *old_parent = data;

	struct sway_container *old_output = old_parent;
	if (old_output != NULL && old_output->type != C_OUTPUT) {
		old_output = container_parent(old_output, C_OUTPUT);
	}

	struct sway_container *new_output = view->swayc->parent;
	if (new_output != NULL && new_output->type != C_OUTPUT) {
		new_output = container_parent(new_output, C_OUTPUT);
	}

	if (old_output == new_output) {
		return;
	}

	if (old_output != NULL) {
		view_for_each_surface(view, surface_send_leave_iterator,
			old_output->sway_output->wlr_output);
	}
	if (new_output != NULL) {
		view_for_each_surface(view, surface_send_enter_iterator,
			new_output->sway_output->wlr_output);
	}
}

static bool view_has_executed_criteria(struct sway_view *view,
		struct criteria *criteria) {
	for (int i = 0; i < view->executed_criteria->length; ++i) {
		struct criteria *item = view->executed_criteria->items[i];
		if (item == criteria) {
			return true;
		}
	}
	return false;
}

void view_execute_criteria(struct sway_view *view) {
	if (!view->swayc) {
		return;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *prior_focus = seat_get_focus(seat);
	list_t *criterias = criteria_for_view(view, CT_COMMAND);
	for (int i = 0; i < criterias->length; i++) {
		struct criteria *criteria = criterias->items[i];
		wlr_log(L_DEBUG, "Checking criteria %s", criteria->raw);
		if (view_has_executed_criteria(view, criteria)) {
			wlr_log(L_DEBUG, "Criteria already executed");
			continue;
		}
		wlr_log(L_DEBUG, "for_window '%s' matches view %p, cmd: '%s'",
				criteria->raw, view, criteria->cmdlist);
		list_add(view->executed_criteria, criteria);
		struct cmd_results *res = execute_command(criteria->cmdlist, NULL);
		if (res->status != CMD_SUCCESS) {
			wlr_log(L_ERROR, "Command '%s' failed: %s", res->input, res->error);
		}
		free_cmd_results(res);
		// view must be focused for commands to affect it,
		// so always refocus in-between command lists
		seat_set_focus(seat, view->swayc);
	}
	list_free(criterias);
	seat_set_focus(seat, prior_focus);
}

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface) {
	if (!sway_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, &root_container);
	struct sway_container *cont = NULL;

	// Check if there's any `assign` criteria for the view
	list_t *criterias = criteria_for_view(view,
			CT_ASSIGN_WORKSPACE | CT_ASSIGN_OUTPUT);
	struct sway_container *workspace = NULL;
	if (criterias->length) {
		struct criteria *criteria = criterias->items[0];
		if (criteria->type == CT_ASSIGN_WORKSPACE) {
			workspace = workspace_by_name(criteria->target);
			if (!workspace) {
				workspace = workspace_create(NULL, criteria->target);
			}
			focus = seat_get_focus_inactive(seat, workspace);
		} else {
			// TODO: CT_ASSIGN_OUTPUT
		}
	}
	// If we're about to launch the view into the floating container, then
	// launch it as a tiled view in the root of the workspace instead.
	if (container_is_floating(focus)) {
		focus = focus->parent->parent;
	}
	free(criterias);
	cont = container_view_create(focus, view);

	view->surface = wlr_surface;
	view->swayc = cont;
	view->border = config->border;
	view->border_thickness = config->border_thickness;
	view->swayc->pending.border = config->border;
	view->swayc->pending.border_thickness = config->border_thickness;

	view_init_subsurfaces(view, wlr_surface);
	wl_signal_add(&wlr_surface->events.new_subsurface,
		&view->surface_new_subsurface);
	view->surface_new_subsurface.notify = view_handle_surface_new_subsurface;

	wl_signal_add(&view->swayc->events.reparent, &view->container_reparent);
	view->container_reparent.notify = view_handle_container_reparent;

	if (view->impl->wants_floating && view->impl->wants_floating(view)) {
		container_set_floating(view->swayc, true);
	} else {
		arrange_children_of(cont->parent);
	}

	input_manager_set_focus(input_manager, cont);
	if (workspace) {
		workspace_switch(workspace);
	}

	view_update_title(view, false);
	container_notify_subtree_changed(view->swayc->parent);
	view_execute_criteria(view);

	container_damage_whole(cont);
	view_handle_container_reparent(&view->container_reparent, NULL);
}

void view_unmap(struct sway_view *view) {
	if (!sway_assert(view->surface != NULL, "cannot unmap unmapped view")) {
		return;
	}

	wl_signal_emit(&view->events.unmap, view);

	if (view->is_fullscreen) {
		struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);
		ws->sway_workspace->fullscreen = NULL;
	}

	container_damage_whole(view->swayc);

	wl_list_remove(&view->surface_new_subsurface.link);
	wl_list_remove(&view->container_reparent.link);

	struct sway_container *parent = container_destroy(view->swayc);

	view->swayc = NULL;
	view->surface = NULL;

	if (view->title_format) {
		free(view->title_format);
		view->title_format = NULL;
	}

	if (parent->type == C_OUTPUT) {
		arrange_output(parent);
	} else {
		arrange_children_of(parent);
	}
}

void view_update_position(struct sway_view *view, double lx, double ly) {
	if (view->x == lx && view->y == ly) {
		return;
	}
	container_damage_whole(view->swayc);
	view->x = lx;
	view->y = ly;
	if (container_is_floating(view->swayc)) {
		container_set_geometry_from_floating_view(view->swayc);
	}
	container_damage_whole(view->swayc);
}

void view_update_size(struct sway_view *view, int width, int height) {
	if (view->width == width && view->height == height) {
		return;
	}
	container_damage_whole(view->swayc);
	view->width = width;
	view->height = height;
	if (container_is_floating(view->swayc)) {
		container_set_geometry_from_floating_view(view->swayc);
	}
	container_damage_whole(view->swayc);
}

static void view_subsurface_create(struct sway_view *view,
		struct wlr_subsurface *subsurface) {
	struct sway_view_child *child = calloc(1, sizeof(struct sway_view_child));
	if (child == NULL) {
		wlr_log(L_ERROR, "Allocation failed");
		return;
	}
	view_child_init(child, NULL, view, subsurface->surface);
}

static void view_child_handle_surface_commit(struct wl_listener *listener,
		void *data) {
	struct sway_view_child *child =
		wl_container_of(listener, child, surface_commit);
	// TODO: only accumulate damage from the child
	view_damage_from(child->view);
}

static void view_child_handle_surface_new_subsurface(
		struct wl_listener *listener, void *data) {
	struct sway_view_child *child =
		wl_container_of(listener, child, surface_new_subsurface);
	struct wlr_subsurface *subsurface = data;
	view_subsurface_create(child->view, subsurface);
}

static void view_child_handle_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_view_child *child =
		wl_container_of(listener, child, surface_destroy);
	view_child_destroy(child);
}

static void view_child_handle_view_unmap(struct wl_listener *listener,
		void *data) {
	struct sway_view_child *child =
		wl_container_of(listener, child, view_unmap);
	view_child_destroy(child);
}

static void view_init_subsurfaces(struct sway_view *view,
		struct wlr_surface *surface) {
	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->subsurfaces, parent_link) {
		view_subsurface_create(view, subsurface);
	}
}

void view_child_init(struct sway_view_child *child,
		const struct sway_view_child_impl *impl, struct sway_view *view,
		struct wlr_surface *surface) {
	child->impl = impl;
	child->view = view;
	child->surface = surface;

	wl_signal_add(&surface->events.commit, &child->surface_commit);
	child->surface_commit.notify = view_child_handle_surface_commit;
	wl_signal_add(&surface->events.new_subsurface,
		&child->surface_new_subsurface);
	child->surface_new_subsurface.notify =
		view_child_handle_surface_new_subsurface;
	wl_signal_add(&surface->events.destroy, &child->surface_destroy);
	child->surface_destroy.notify = view_child_handle_surface_destroy;
	wl_signal_add(&view->events.unmap, &child->view_unmap);
	child->view_unmap.notify = view_child_handle_view_unmap;

	struct sway_container *output = child->view->swayc->parent;
	if (output != NULL) {
		if (output->type != C_OUTPUT) {
			output = container_parent(output, C_OUTPUT);
		}
		wlr_surface_send_enter(child->surface, output->sway_output->wlr_output);
	}

	view_init_subsurfaces(child->view, surface);

	// TODO: only damage the whole child
	if (child->view->swayc) {
		container_damage_whole(child->view->swayc);
	}
}

void view_child_destroy(struct sway_view_child *child) {
	// TODO: only damage the whole child
	if (child->view->swayc) {
		container_damage_whole(child->view->swayc);
	}

	wl_list_remove(&child->surface_commit.link);
	wl_list_remove(&child->surface_destroy.link);
	wl_list_remove(&child->view_unmap.link);

	if (child->impl && child->impl->destroy) {
		child->impl->destroy(child);
	} else {
		free(child);
	}
}

static size_t append_prop(char *buffer, const char *value) {
	if (!value) {
		return 0;
	}
	lenient_strcat(buffer, value);
	return strlen(value);
}

/**
 * Calculate and return the length of the formatted title.
 * If buffer is not NULL, also populate the buffer with the formatted title.
 */
static size_t parse_title_format(struct sway_view *view, char *buffer) {
	if (!view->title_format || strcmp(view->title_format, "%title") == 0) {
		const char *title = view_get_title(view);
		if (buffer && title) {
			strcpy(buffer, title);
		}
		return title ? strlen(title) : 0;
	}

	size_t len = 0;
	char *format = view->title_format;
	char *next = strchr(format, '%');
	while (next) {
		// Copy everything up to the %
		lenient_strncat(buffer, format, next - format);
		len += next - format;
		format = next;

		if (strncmp(next, "%title", 6) == 0) {
			len += append_prop(buffer, view_get_title(view));
			format += 6;
		} else if (strncmp(next, "%app_id", 7) == 0) {
			len += append_prop(buffer, view_get_app_id(view));
			format += 7;
		} else if (strncmp(next, "%class", 6) == 0) {
			len += append_prop(buffer, view_get_class(view));
			format += 6;
		} else if (strncmp(next, "%instance", 9) == 0) {
			len += append_prop(buffer, view_get_instance(view));
			format += 9;
		} else if (strncmp(next, "%shell", 6) == 0) {
			len += append_prop(buffer, view_get_shell(view));
			format += 6;
		} else {
			lenient_strcat(buffer, "%");
			++format;
			++len;
		}
		next = strchr(format, '%');
	}
	lenient_strcat(buffer, format);
	len += strlen(format);

	return len;
}

static char *escape_title(char *buffer) {
	int length = escape_markup_text(buffer, NULL, 0);
	char *escaped_title = calloc(length + 1, sizeof(char));
	int result = escape_markup_text(buffer, escaped_title, length);
	if (result != length) {
		wlr_log(L_ERROR, "Could not escape title: %s", buffer);
		free(escaped_title);
		return buffer;
	}
	free(buffer);
	return escaped_title;
}

void view_update_title(struct sway_view *view, bool force) {
	if (!view->swayc) {
		return;
	}
	const char *title = view_get_title(view);

	if (!force) {
		if (title && view->swayc->name && strcmp(title, view->swayc->name) == 0) {
			return;
		}
		if (!title && !view->swayc->name) {
			return;
		}
	}

	free(view->swayc->name);
	free(view->swayc->formatted_title);
	if (title) {
		size_t len = parse_title_format(view, NULL);
		char *buffer = calloc(len + 1, sizeof(char));
		if (!sway_assert(buffer, "Unable to allocate title string")) {
			return;
		}
		parse_title_format(view, buffer);
		// now we have the title, but needs to be escaped when using pango markup
		if (config->pango_markup) {
			buffer = escape_title(buffer);
		}

		view->swayc->name = strdup(title);
		view->swayc->formatted_title = buffer;
	} else {
		view->swayc->name = NULL;
		view->swayc->formatted_title = NULL;
	}
	container_calculate_title_height(view->swayc);
	config_update_font_height(false);

	// Update title after the global font height is updated
	container_update_title_textures(view->swayc);
}

static bool find_by_mark_iterator(struct sway_container *con,
		void *data) {
	char *mark = data;
	return con->type == C_VIEW && view_has_mark(con->sway_view, mark);
}

bool view_find_and_unmark(char *mark) {
	struct sway_container *container = container_find(&root_container,
		find_by_mark_iterator, mark);
	if (!container) {
		return false;
	}
	struct sway_view *view = container->sway_view;

	for (int i = 0; i < view->marks->length; ++i) {
		char *view_mark = view->marks->items[i];
		if (strcmp(view_mark, mark) == 0) {
			free(view_mark);
			list_del(view->marks, i);
			view_update_marks_textures(view);
			return true;
		}
	}
	return false;
}

void view_clear_marks(struct sway_view *view) {
	for (int i = 0; i < view->marks->length; ++i) {
		free(view->marks->items[i]);
	}
	list_free(view->marks);
	view->marks = create_list();
}

bool view_has_mark(struct sway_view *view, char *mark) {
	for (int i = 0; i < view->marks->length; ++i) {
		char *item = view->marks->items[i];
		if (strcmp(item, mark) == 0) {
			return true;
		}
	}
	return false;
}

static void update_marks_texture(struct sway_view *view,
		struct wlr_texture **texture, struct border_colors *class) {
	struct sway_container *output = container_parent(view->swayc, C_OUTPUT);
	if (!output) {
		return;
	}
	if (*texture) {
		wlr_texture_destroy(*texture);
		*texture = NULL;
	}
	if (!view->marks->length) {
		return;
	}

	size_t len = 0;
	for (int i = 0; i < view->marks->length; ++i) {
		char *mark = view->marks->items[i];
		if (mark[0] != '_') {
			len += strlen(mark) + 2;
		}
	}
	char *buffer = calloc(len + 1, 1);
	char *part = malloc(len + 1);

	if (!sway_assert(buffer && part, "Unable to allocate memory")) {
		free(buffer);
		return;
	}

	for (int i = 0; i < view->marks->length; ++i) {
		char *mark = view->marks->items[i];
		if (mark[0] != '_') {
			sprintf(part, "[%s]", mark);
			strcat(buffer, part);
		}
	}
	free(part);

	double scale = output->sway_output->wlr_output->scale;
	int width = 0;
	int height = config->font_height * scale;

	cairo_t *c = cairo_create(NULL);
	get_text_size(c, config->font, &width, NULL, scale, false, "%s", buffer);
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

	pango_printf(cairo, config->font, scale, false, "%s", buffer);

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
	free(buffer);
}

void view_update_marks_textures(struct sway_view *view) {
	if (!config->show_marks) {
		return;
	}
	update_marks_texture(view, &view->marks_focused,
			&config->border_colors.focused);
	update_marks_texture(view, &view->marks_focused_inactive,
			&config->border_colors.focused_inactive);
	update_marks_texture(view, &view->marks_unfocused,
			&config->border_colors.unfocused);
	update_marks_texture(view, &view->marks_urgent,
			&config->border_colors.urgent);
	container_damage_whole(view->swayc);
}

bool view_is_visible(struct sway_view *view) {
	if (!view->swayc) {
		return false;
	}
	struct sway_container *workspace =
		container_parent(view->swayc, C_WORKSPACE);
	// Determine if view is nested inside a floating container which is sticky.
	// A simple floating view will have this ancestry:
	// C_VIEW -> floating -> workspace
	// A more complex ancestry could be:
	// C_VIEW -> C_CONTAINER (tabbed) -> floating -> workspace
	struct sway_container *floater = view->swayc;
	while (floater->parent->type != C_WORKSPACE
			&& floater->parent->parent->type != C_WORKSPACE) {
		floater = floater->parent;
	}
	bool is_sticky = container_is_floating(floater) && floater->is_sticky;
	// Check view isn't in a tabbed or stacked container on an inactive tab
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *container = view->swayc;
	while (container->type != C_WORKSPACE) {
		if (container->parent->layout == L_TABBED ||
				container->parent->layout == L_STACKED) {
			if (seat_get_active_child(seat, container->parent) != container) {
				return false;
			}
		}
		container = container->parent;
	}
	// Check view isn't hidden by another fullscreen view
	if (workspace->sway_workspace->fullscreen && !view->is_fullscreen) {
		return false;
	}
	// Check the workspace is visible
	if (!is_sticky) {
	return workspace_is_visible(workspace);
	}
	return true;
}
