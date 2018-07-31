#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_layout.h>
#include "config.h"
#ifdef HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif
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
	view->allow_request_urgent = true;
	wl_signal_init(&view->events.unmap);
}

void view_free(struct sway_view *view) {
	if (!sway_assert(view->surface == NULL, "Tried to free mapped view")) {
		return;
	}
	if (!sway_assert(view->destroying,
				"Tried to free view which wasn't marked as destroying")) {
		return;
	}
	if (!sway_assert(view->swayc == NULL,
				"Tried to free view which still has a swayc "
				"(might have a pending transaction?)")) {
		return;
	}
	list_free(view->executed_criteria);

	list_foreach(view->marks, free);
	list_free(view->marks);

	wlr_texture_destroy(view->marks_focused);
	wlr_texture_destroy(view->marks_focused_inactive);
	wlr_texture_destroy(view->marks_unfocused);
	wlr_texture_destroy(view->marks_urgent);

	if (view->impl->destroy) {
		view->impl->destroy(view);
	} else {
		free(view);
	}
}

/**
 * The view may or may not be involved in a transaction. For example, a view may
 * unmap then attempt to destroy itself before we've applied the new layout. If
 * an unmapping view is still involved in a transaction then it'll still have a
 * swayc.
 *
 * If there's no transaction we can simply free the view. Otherwise the
 * destroying flag will make the view get freed when the transaction is
 * finished.
 */
void view_destroy(struct sway_view *view) {
	if (!sway_assert(view->surface == NULL, "Tried to destroy a mapped view")) {
		return;
	}
	view->destroying = true;

	if (!view->swayc) {
		view_free(view);
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
#ifdef HAVE_XWAYLAND
uint32_t view_get_x11_window_id(struct sway_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_X11_WINDOW_ID);
	}
	return 0;
}
#endif
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
#ifdef HAVE_XWAYLAND
	case SWAY_VIEW_XWAYLAND:
		return "xwayland";
#endif
	}
	return "unknown";
}

void view_get_constraints(struct sway_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height) {
	if (view->impl->get_constraints) {
		view->impl->get_constraints(view,
				min_width, max_width, min_height, max_height);
	} else {
		*min_width = DBL_MIN;
		*max_width = DBL_MAX;
		*min_height = DBL_MIN;
		*max_height = DBL_MAX;
	}
}

uint32_t view_configure(struct sway_view *view, double lx, double ly, int width,
		int height) {
	if (view->impl->configure) {
		return view->impl->configure(view, lx, ly, width, height);
	}
	return 0;
}

void view_autoconfigure(struct sway_view *view) {
	if (!sway_assert(view->swayc,
				"Called view_autoconfigure() on a view without a swayc")) {
		return;
	}

	struct sway_container *output = container_parent(view->swayc, C_OUTPUT);

	if (view->swayc->is_fullscreen) {
		view->x = output->x;
		view->y = output->y;
		view->width = output->width;
		view->height = output->height;
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

	struct sway_container *con = view->swayc;

	view->border_top = view->border_bottom = true;
	view->border_left = view->border_right = true;
	if (config->hide_edge_borders == E_BOTH
			|| config->hide_edge_borders == E_VERTICAL
			|| (config->hide_edge_borders == E_SMART && !other_views)) {
		view->border_left = con->x != ws->x;
		int right_x = con->x + con->width;
		view->border_right = right_x != ws->x + ws->width;
	}
	if (config->hide_edge_borders == E_BOTH
			|| config->hide_edge_borders == E_HORIZONTAL
			|| (config->hide_edge_borders == E_SMART && !other_views)) {
		view->border_top = con->y != ws->y;
		int bottom_y = con->y + con->height;
		view->border_bottom = bottom_y != ws->y + ws->height;
	}

	double x, y, width, height;
	x = y = width = height = 0;
	double y_offset = 0;

	// In a tabbed or stacked container, the swayc's y is the top of the title
	// area. We have to offset the surface y by the height of the title bar, and
	// disable any top border because we'll always have the title bar.
	if (con->parent->layout == L_TABBED) {
		y_offset = container_titlebar_height();
		view->border_top = false;
	} else if (con->parent->layout == L_STACKED) {
		y_offset = container_titlebar_height() * con->parent->children->length;
		view->border_top = false;
	}

	switch (view->border) {
	case B_NONE:
		x = con->x;
		y = con->y + y_offset;
		width = con->width;
		height = con->height - y_offset;
		break;
	case B_PIXEL:
		x = con->x + view->border_thickness * view->border_left;
		y = con->y + view->border_thickness * view->border_top + y_offset;
		width = con->width
			- view->border_thickness * view->border_left
			- view->border_thickness * view->border_right;
		height = con->height - y_offset
			- view->border_thickness * view->border_top
			- view->border_thickness * view->border_bottom;
		break;
	case B_NORMAL:
		// Height is: 1px border + 3px pad + title height + 3px pad + 1px border
		x = con->x + view->border_thickness * view->border_left;
		width = con->width
			- view->border_thickness * view->border_left
			- view->border_thickness * view->border_right;
		if (y_offset) {
			y = con->y + y_offset;
			height = con->height - y_offset
				- view->border_thickness * view->border_bottom;
		} else {
			y = con->y + container_titlebar_height();
			height = con->height - container_titlebar_height()
				- view->border_thickness * view->border_bottom;
		}
		break;
	}

	view->x = x;
	view->y = y;
	view->width = width;
	view->height = height;
}

void view_set_activated(struct sway_view *view, bool activated) {
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
}

void view_set_tiled(struct sway_view *view, bool tiled) {
	if (!tiled) {
		view->using_csd = true;
		if (view->impl->has_client_side_decorations) {
			view->using_csd = view->impl->has_client_side_decorations(view);
		}
	} else {
		view->using_csd = false;
	}

	if (view->impl->set_tiled) {
		view->impl->set_tiled(view, tiled);
	}
}

void view_close(struct sway_view *view) {
	if (view->impl->close) {
		view->impl->close(view);
	}
}

void view_close_popups(struct sway_view *view) {
	if (view->impl->close_popups) {
		view->impl->close_popups(view);
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
	if (!view->surface) {
		return;
	}
	if (view->impl->for_each_surface) {
		view->impl->for_each_surface(view, iterator, user_data);
	} else {
		wlr_surface_for_each_surface(view->surface, iterator, user_data);
	}
}

void view_for_each_popup(struct sway_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	if (!view->surface) {
		return;
	}
	if (view->impl->for_each_popup) {
		view->impl->for_each_popup(view, iterator, user_data);
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
		wlr_log(WLR_DEBUG, "Checking criteria %s", criteria->raw);
		if (view_has_executed_criteria(view, criteria)) {
			wlr_log(WLR_DEBUG, "Criteria already executed");
			continue;
		}
		wlr_log(WLR_DEBUG, "for_window '%s' matches view %p, cmd: '%s'",
				criteria->raw, view, criteria->cmdlist);
		seat_set_focus(seat, view->swayc);
		list_add(view->executed_criteria, criteria);
		struct cmd_results *res = execute_command(criteria->cmdlist, NULL);
		if (res->status != CMD_SUCCESS) {
			wlr_log(WLR_ERROR, "Command '%s' failed: %s", res->input, res->error);
		}
		free_cmd_results(res);
	}
	list_free(criterias);
	seat_set_focus(seat, prior_focus);
}

static struct sway_container *select_workspace(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	// Check if there's any `assign` criteria for the view
	list_t *criterias = criteria_for_view(view,
			CT_ASSIGN_WORKSPACE | CT_ASSIGN_OUTPUT);
	struct sway_container *ws = NULL;
	for (int i = 0; i < criterias->length; ++i) {
		struct criteria *criteria = criterias->items[i];
		if (criteria->type == CT_ASSIGN_WORKSPACE) {
			ws = workspace_by_name(criteria->target);
			if (!ws) {
				ws = workspace_create(NULL, criteria->target);
			}
			break;
		} else {
			// CT_ASSIGN_OUTPUT
			struct sway_container *output = output_by_name(criteria->target);
			if (output) {
				ws = seat_get_active_child(seat, output);
				break;
			}
		}
	}
	list_free(criterias);
	if (ws) {
		return ws;
	}

	// Check if there's a PID mapping
	pid_t pid;
#ifdef HAVE_XWAYLAND
	if (view->type == SWAY_VIEW_XWAYLAND) {
		struct wlr_xwayland_surface *surf =
			wlr_xwayland_surface_from_wlr_surface(view->surface);
		pid = surf->pid;
	} else {
		struct wl_client *client =
			wl_resource_get_client(view->surface->resource);
		wl_client_get_credentials(client, &pid, NULL, NULL);
	}
#else
	struct wl_client *client =
		wl_resource_get_client(view->surface->resource);
	wl_client_get_credentials(client, &pid, NULL, NULL);
#endif
	ws = workspace_for_pid(pid);
	if (ws) {
		return ws;
	}

	// Use the focused workspace
	ws = seat_get_focus(seat);
	if (ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}
	return ws;
}

static bool should_focus(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *prev_focus = seat_get_focus(seat);
	struct sway_container *prev_ws = prev_focus->type == C_WORKSPACE ?
		prev_focus : container_parent(prev_focus, C_WORKSPACE);
	struct sway_container *map_ws = container_parent(view->swayc, C_WORKSPACE);

	// Views can only take focus if they are mapped into the active workspace
	if (prev_ws != map_ws) {
		return false;
	}

	// If the view is the only one in the focused workspace, it'll get focus
	// regardless of any no_focus criteria.
	struct sway_container *parent = view->swayc->parent;
	if (parent->type == C_WORKSPACE && prev_focus == parent) {
		size_t num_children = parent->children->length +
			parent->sway_workspace->floating->children->length;
		if (num_children == 1) {
			return true;
		}
	}

	// Check no_focus criteria
	list_t *criterias = criteria_for_view(view, CT_NO_FOCUS);
	size_t len = criterias->length;
	list_free(criterias);
	return len == 0;
}

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface) {
	if (!sway_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}
	view->surface = wlr_surface;

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *ws = select_workspace(view);
	struct sway_container *target_sibling = seat_get_focus_inactive(seat, ws);

	// If we're about to launch the view into the floating container, then
	// launch it as a tiled view in the root of the workspace instead.
	if (container_is_floating(target_sibling)) {
		target_sibling = target_sibling->parent->parent;
	}

	view->swayc = container_view_create(target_sibling, view);

	view_init_subsurfaces(view, wlr_surface);
	wl_signal_add(&wlr_surface->events.new_subsurface,
		&view->surface_new_subsurface);
	view->surface_new_subsurface.notify = view_handle_surface_new_subsurface;

	wl_signal_add(&view->swayc->events.reparent, &view->container_reparent);
	view->container_reparent.notify = view_handle_container_reparent;

	if (view->impl->wants_floating && view->impl->wants_floating(view)) {
		view->border = config->floating_border;
		view->border_thickness = config->floating_border_thickness;
		container_set_floating(view->swayc, true);
	} else {
		view->border = config->border;
		view->border_thickness = config->border_thickness;
		view_set_tiled(view, true);
	}

	if (should_focus(view)) {
		input_manager_set_focus(input_manager, view->swayc);
	}

	view_update_title(view, false);
	container_notify_subtree_changed(view->swayc->parent);
	view_execute_criteria(view);

	view_handle_container_reparent(&view->container_reparent, NULL);
}

void view_unmap(struct sway_view *view) {
	wl_signal_emit(&view->events.unmap, view);

	wl_list_remove(&view->surface_new_subsurface.link);
	wl_list_remove(&view->container_reparent.link);

	if (view->urgent_timer) {
		wl_event_source_remove(view->urgent_timer);
		view->urgent_timer = NULL;
	}

	struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);

	struct sway_container *parent;
	if (container_is_fullscreen_or_child(view->swayc)) {
		parent = container_destroy(view->swayc);
		arrange_windows(ws->parent);
	} else {
		parent = container_destroy(view->swayc);
		arrange_windows(parent);
	}
	if (parent->type >= C_WORKSPACE) { // if the workspace still exists
		workspace_detect_urgent(ws);
	}
	transaction_commit_dirty();
	view->surface = NULL;
}

void view_update_position(struct sway_view *view, double lx, double ly) {
	if (view->x == lx && view->y == ly) {
		return;
	}
	container_damage_whole(view->swayc);
	view->x = lx;
	view->y = ly;
	view->swayc->current.view_x = lx;
	view->swayc->current.view_y = ly;
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
	view->swayc->current.view_width = width;
	view->swayc->current.view_height = height;
	if (container_is_floating(view->swayc)) {
		container_set_geometry_from_floating_view(view->swayc);
	}
	container_damage_whole(view->swayc);
}

static void view_subsurface_create(struct sway_view *view,
		struct wlr_subsurface *subsurface) {
	struct sway_view_child *child = calloc(1, sizeof(struct sway_view_child));
	if (child == NULL) {
		wlr_log(WLR_ERROR, "Allocation failed");
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

struct sway_view *view_from_wlr_surface(struct wlr_surface *wlr_surface) {
	if (wlr_surface_is_xdg_surface(wlr_surface)) {
		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_from_wlr_surface(wlr_surface);
		return view_from_wlr_xdg_surface(xdg_surface);
	}
	if (wlr_surface_is_xdg_surface_v6(wlr_surface)) {
		struct wlr_xdg_surface_v6 *xdg_surface_v6 =
			wlr_xdg_surface_v6_from_wlr_surface(wlr_surface);
		return view_from_wlr_xdg_surface_v6(xdg_surface_v6);
	}
#ifdef HAVE_XWAYLAND
	if (wlr_surface_is_xwayland_surface(wlr_surface)) {
		struct wlr_xwayland_surface *xsurface =
			wlr_xwayland_surface_from_wlr_surface(wlr_surface);
		return view_from_wlr_xwayland_surface(xsurface);
	}
#endif
	if (wlr_surface_is_subsurface(wlr_surface)) {
		struct wlr_subsurface *subsurface =
			wlr_subsurface_from_wlr_surface(wlr_surface);
		return view_from_wlr_surface(subsurface->parent);
	}
	if (wlr_surface_is_layer_surface(wlr_surface)) {
		return NULL;
	}

	const char *role = wlr_surface->role ? wlr_surface->role->name : NULL;
	wlr_log(WLR_DEBUG, "Surface of unknown type (role %s): %p",
		role, wlr_surface);
	return NULL;
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
		wlr_log(WLR_ERROR, "Could not escape title: %s", buffer);
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
	int height = view->swayc->title_height * scale;

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
	if (!view->swayc || view->swayc->destroying) {
		return false;
	}
	struct sway_container *workspace =
		container_parent(view->swayc, C_WORKSPACE);
	if (!workspace) {
		return false;
	}
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
	while (container->type != C_WORKSPACE && container->layout != L_FLOATING) {
		if (container->parent->layout == L_TABBED ||
				container->parent->layout == L_STACKED) {
			if (seat_get_active_child(seat, container->parent) != container) {
				return false;
			}
		}
		container = container->parent;
	}
	// Check view isn't hidden by another fullscreen view
	if (workspace->sway_workspace->fullscreen &&
			!container_is_fullscreen_or_child(view->swayc)) {
		return false;
	}
	// Check the workspace is visible
	if (!is_sticky) {
		return workspace_is_visible(workspace);
	}
	return true;
}

void view_set_urgent(struct sway_view *view, bool enable) {
	if (view_is_urgent(view) == enable) {
		return;
	}
	if (enable) {
		struct sway_seat *seat = input_manager_current_seat(input_manager);
		if (seat_get_focus(seat) == view->swayc) {
			return;
		}
		clock_gettime(CLOCK_MONOTONIC, &view->urgent);
	} else {
		view->urgent = (struct timespec){ 0 };
		if (view->urgent_timer) {
			wl_event_source_remove(view->urgent_timer);
			view->urgent_timer = NULL;
		}
	}
	container_damage_whole(view->swayc);

	ipc_event_window(view->swayc, "urgent");

	struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);
	workspace_detect_urgent(ws);
}

bool view_is_urgent(struct sway_view *view) {
	return view->urgent.tv_sec || view->urgent.tv_nsec;
}
