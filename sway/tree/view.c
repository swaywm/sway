#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include "config.h"
#ifdef HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include "list.h"
#include "log.h"
#include "sway/criteria.h"
#include "sway/commands.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/input/seat.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
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

void view_destroy(struct sway_view *view) {
	if (!sway_assert(view->surface == NULL, "Tried to free mapped view")) {
		return;
	}
	if (!sway_assert(view->destroying,
				"Tried to free view which wasn't marked as destroying")) {
		return;
	}
	if (!sway_assert(view->container == NULL,
				"Tried to free view which still has a container "
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
	free(view->title_format);

	if (view->impl->destroy) {
		view->impl->destroy(view);
	} else {
		free(view);
	}
}

void view_begin_destroy(struct sway_view *view) {
	if (!sway_assert(view->surface == NULL, "Tried to destroy a mapped view")) {
		return;
	}
	view->destroying = true;

	if (!view->container) {
		view_destroy(view);
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
	if (!view->container->workspace) {
		// Hidden in the scratchpad
		return;
	}
	struct sway_output *output = view->container->workspace->output;

	if (view->container->is_fullscreen) {
		view->x = output->lx;
		view->y = output->ly;
		view->width = output->width;
		view->height = output->height;
		return;
	}

	struct sway_workspace *ws = view->container->workspace;

	bool other_views = false;
	if (config->hide_edge_borders == E_SMART) {
		struct sway_container *con = view->container;
		while (con) {
			enum sway_container_layout layout = container_parent_layout(con);
			if (layout != L_TABBED && layout != L_STACKED) {
				list_t *siblings = container_get_siblings(con);
				if (siblings && siblings->length > 1) {
					other_views = true;
					break;
				}
			}
			con = con->parent;
		}
	}

	struct sway_container *con = view->container;

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

	// In a tabbed or stacked container, the container's y is the top of the
	// title area. We have to offset the surface y by the height of the title,
	// bar, and disable any top border because we'll always have the title bar.
	enum sway_container_layout layout = container_parent_layout(con);
	if (layout == L_TABBED) {
		y_offset = container_titlebar_height();
		view->border_top = false;
	} else if (layout == L_STACKED) {
		list_t *siblings = container_get_siblings(con);
		y_offset = container_titlebar_height() * siblings->length;
		view->border_top = false;
	}

	enum sway_container_border border = view->border;
	if (view->using_csd) {
		border = B_NONE;
	}

	switch (border) {
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

void view_request_activate(struct sway_view *view) {
	struct sway_workspace *ws = view->container->workspace;
	if (!ws) { // hidden scratchpad container
		return;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	switch (config->focus_on_window_activation) {
	case FOWA_SMART:
		if (workspace_is_visible(ws)) {
			seat_set_focus_container(seat, view->container);
		} else {
			view_set_urgent(view, true);
		}
		break;
	case FOWA_URGENT:
		view_set_urgent(view, true);
		break;
	case FOWA_FOCUS:
		seat_set_focus_container(seat, view->container);
		break;
	case FOWA_NONE:
		break;
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
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_from_view(output, view);
	}
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
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_node *prior_focus = seat_get_focus(seat);
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
		seat_set_focus_container(seat, view->container);
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

static struct sway_workspace *select_workspace(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	// Check if there's any `assign` criteria for the view
	list_t *criterias = criteria_for_view(view,
			CT_ASSIGN_WORKSPACE | CT_ASSIGN_WORKSPACE_NUMBER | CT_ASSIGN_OUTPUT);
	struct sway_workspace *ws = NULL;
	for (int i = 0; i < criterias->length; ++i) {
		struct criteria *criteria = criterias->items[i];
		if (criteria->type == CT_ASSIGN_OUTPUT) {
			struct sway_output *output = output_by_name(criteria->target);
			if (output) {
				ws = output_get_active_workspace(output);
				break;
			}
		} else {
			// CT_ASSIGN_WORKSPACE(_NUMBER)
			ws = criteria->type == CT_ASSIGN_WORKSPACE_NUMBER ?
				workspace_by_number(criteria->target) :
				workspace_by_name(criteria->target);

			if (!ws) {
				if (strcasecmp(criteria->target, "back_and_forth") == 0) {
					if (prev_workspace_name) {
						ws = workspace_create(NULL, prev_workspace_name);
					}
				} else {
					ws = workspace_create(NULL, criteria->target);
				}
			}
			break;
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
	view->pid = pid;
	ws = root_workspace_for_pid(pid);
	if (ws) {
		return ws;
	}

	// Use the focused workspace
	return seat_get_focused_workspace(seat);
}

static bool should_focus(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *prev_con = seat_get_focused_container(seat);
	struct sway_workspace *prev_ws = seat_get_focused_workspace(seat);
	struct sway_workspace *map_ws = view->container->workspace;

	// Views can only take focus if they are mapped into the active workspace
	if (prev_ws != map_ws) {
		return false;
	}

	// If the view is the only one in the focused workspace, it'll get focus
	// regardless of any no_focus criteria.
	if (!view->container->parent && !prev_con) {
		size_t num_children = view->container->workspace->tiling->length +
			view->container->workspace->floating->length;
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
	struct sway_workspace *ws = select_workspace(view);
	struct sway_node *node = seat_get_focus_inactive(seat, &ws->node);
	struct sway_container *target_sibling = node->type == N_CONTAINER ?
		node->sway_container : NULL;

	// If we're about to launch the view into the floating container, then
	// launch it as a tiled view in the root of the workspace instead.
	if (target_sibling && container_is_floating(target_sibling)) {
		target_sibling = NULL;
	}

	view->container = container_create(view);
	if (target_sibling) {
		container_add_sibling(target_sibling, view->container, 1);
	} else {
		workspace_add_tiling(ws, view->container);
	}
	ipc_event_window(view->container, "new");

	view_init_subsurfaces(view, wlr_surface);
	wl_signal_add(&wlr_surface->events.new_subsurface,
		&view->surface_new_subsurface);
	view->surface_new_subsurface.notify = view_handle_surface_new_subsurface;

	if (view->impl->wants_floating && view->impl->wants_floating(view)) {
		view->border = config->floating_border;
		view->border_thickness = config->floating_border_thickness;
		container_set_floating(view->container, true);
	} else {
		view->border = config->border;
		view->border_thickness = config->border_thickness;
		view_set_tiled(view, true);
	}

	if (should_focus(view)) {
		input_manager_set_focus(input_manager, &view->container->node);
	}

	view_update_title(view, false);
	container_update_representation(view->container);
	view_execute_criteria(view);
}

void view_unmap(struct sway_view *view) {
	wl_signal_emit(&view->events.unmap, view);

	wl_list_remove(&view->surface_new_subsurface.link);

	if (view->urgent_timer) {
		wl_event_source_remove(view->urgent_timer);
		view->urgent_timer = NULL;
	}

	struct sway_container *parent = view->container->parent;
	struct sway_workspace *ws = view->container->workspace;
	container_begin_destroy(view->container);
	if (parent) {
		container_reap_empty(parent);
	} else if (ws) {
		workspace_consider_destroy(ws);
	}

	if (ws && !ws->node.destroying) {
		arrange_workspace(ws);
		workspace_detect_urgent(ws);
	}

	struct sway_seat *seat;
	wl_list_for_each(seat, &input_manager->seats, link) {
		cursor_send_pointer_motion(seat->cursor, 0, true);
	}

	transaction_commit_dirty();
	view->surface = NULL;
}

void view_update_size(struct sway_view *view, int width, int height) {
	if (!sway_assert(container_is_floating(view->container),
				"Expected a floating container")) {
		return;
	}
	view->width = width;
	view->height = height;
	view->container->current.view_width = width;
	view->container->current.view_height = height;
	container_set_geometry_from_floating_view(view->container);
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

	struct sway_output *output = child->view->container->workspace->output;
	wlr_surface_send_enter(child->surface, output->wlr_output);

	view_init_subsurfaces(child->view, surface);

	// TODO: only damage the whole child
	container_damage_whole(child->view->container);
}

void view_child_destroy(struct sway_view_child *child) {
	// TODO: only damage the whole child
	container_damage_whole(child->view->container);

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
	const char *title = view_get_title(view);

	if (!force) {
		if (title && view->container->title &&
				strcmp(title, view->container->title) == 0) {
			return;
		}
		if (!title && !view->container->title) {
			return;
		}
	}

	free(view->container->title);
	free(view->container->formatted_title);
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

		view->container->title = strdup(title);
		view->container->formatted_title = buffer;
	} else {
		view->container->title = NULL;
		view->container->formatted_title = NULL;
	}
	container_calculate_title_height(view->container);
	config_update_font_height(false);

	// Update title after the global font height is updated
	container_update_title_textures(view->container);

	ipc_event_window(view->container, "title");
}

static bool find_by_mark_iterator(struct sway_container *con,
		void *data) {
	char *mark = data;
	return con->view && view_has_mark(con->view, mark);
}

struct sway_view *view_find_mark(char *mark) {
	struct sway_container *container = root_find_container(
			find_by_mark_iterator, mark);
	if (!container) {
		return NULL;
	}
	return container->view;
}

bool view_find_and_unmark(char *mark) {
	struct sway_container *container = root_find_container(
		find_by_mark_iterator, mark);
	if (!container) {
		return false;
	}
	struct sway_view *view = container->view;

	for (int i = 0; i < view->marks->length; ++i) {
		char *view_mark = view->marks->items[i];
		if (strcmp(view_mark, mark) == 0) {
			free(view_mark);
			list_del(view->marks, i);
			view_update_marks_textures(view);
			ipc_event_window(container, "mark");
			return true;
		}
	}
	return false;
}

void view_clear_marks(struct sway_view *view) {
	list_foreach(view->marks, free);
	view->marks->length = 0;
	ipc_event_window(view->container, "mark");
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

void view_add_mark(struct sway_view *view, char *mark) {
	list_add(view->marks, strdup(mark));
	ipc_event_window(view->container, "mark");
}

static void update_marks_texture(struct sway_view *view,
		struct wlr_texture **texture, struct border_colors *class) {
	struct sway_output *output =
		container_get_effective_output(view->container);
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

	double scale = output->wlr_output->scale;
	int width = 0;
	int height = view->container->title_height * scale;

	cairo_t *c = cairo_create(NULL);
	get_text_size(c, config->font, &width, NULL, NULL, scale, false,
			"%s", buffer);
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
			output->wlr_output->backend);
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
	container_damage_whole(view->container);
}

bool view_is_visible(struct sway_view *view) {
	if (view->container->node.destroying) {
		return false;
	}
	struct sway_workspace *workspace = view->container->workspace;
	if (!workspace) {
		return false;
	}
	// Determine if view is nested inside a floating container which is sticky
	struct sway_container *floater = view->container;
	while (floater->parent) {
		floater = floater->parent;
	}
	bool is_sticky = container_is_floating(floater) && floater->is_sticky;
	// Check view isn't in a tabbed or stacked container on an inactive tab
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *con = view->container;
	while (con) {
		enum sway_container_layout layout = container_parent_layout(con);
		if (layout == L_TABBED || layout == L_STACKED) {
			struct sway_node *parent = con->parent ?
				&con->parent->node : &con->workspace->node;
			if (seat_get_active_tiling_child(seat, parent) != &con->node) {
				return false;
			}
		}
		con = con->parent;
	}
	// Check view isn't hidden by another fullscreen view
	if (workspace->fullscreen &&
			!container_is_fullscreen_or_child(view->container)) {
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
		if (seat_get_focused_container(seat) == view->container) {
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
	container_damage_whole(view->container);

	ipc_event_window(view->container, "urgent");

	if (view->container->workspace) {
		workspace_detect_urgent(view->container->workspace);
	}
}

bool view_is_urgent(struct sway_view *view) {
	return view->urgent.tv_sec || view->urgent.tv_nsec;
}

void view_remove_saved_buffer(struct sway_view *view) {
	if (!sway_assert(view->saved_buffer, "Expected a saved buffer")) {
		return;
	}
	wlr_buffer_unref(view->saved_buffer);
	view->saved_buffer = NULL;
}

void view_save_buffer(struct sway_view *view) {
	if (!sway_assert(!view->saved_buffer, "Didn't expect saved buffer")) {
		view_remove_saved_buffer(view);
	}
	if (view->surface && wlr_surface_has_buffer(view->surface)) {
		view->saved_buffer = wlr_buffer_ref(view->surface->buffer);
		view->saved_buffer_width = view->surface->current.width;
		view->saved_buffer_height = view->surface->current.height;
	}
}
