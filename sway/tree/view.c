#include <stdlib.h>
#include <strings.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include "list.h"
#include "log.h"
#include "sway/criteria.h"
#include "sway/commands.h"
#include "sway/desktop/transaction.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/desktop/launcher.h"
#include "sway/input/cursor.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/input/seat.h"
#include "sway/scene_descriptor.h"
#include "sway/server.h"
#include "sway/sway_text_node.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/config.h"
#include "sway/xdg_decoration.h"
#include "stringop.h"

bool view_init(struct sway_view *view, enum sway_view_type type,
		const struct sway_view_impl *impl) {
	bool failed = false;
	view->scene_tree = alloc_scene_tree(root->staging, &failed);
	view->content_tree = alloc_scene_tree(view->scene_tree, &failed);

	if (!failed && !scene_descriptor_assign(&view->scene_tree->node,
			SWAY_SCENE_DESC_VIEW, view)) {
		failed = true;
	}

	view->image_capture_scene = wlr_scene_create();
	if (view->image_capture_scene == NULL) {
		failed = true;
	}

	if (failed) {
		wlr_scene_node_destroy(&view->scene_tree->node);
		return false;
	}

	view->type = type;
	view->impl = impl;
	view->executed_criteria = create_list();
	view->allow_request_urgent = true;
	view->shortcuts_inhibit = SHORTCUTS_INHIBIT_DEFAULT;
	view->tearing_mode = TEARING_WINDOW_HINT;
	wl_signal_init(&view->events.unmap);
	return true;
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
	wl_list_remove(&view->events.unmap.listener_list);
	list_free(view->executed_criteria);

	view_assign_ctx(view, NULL);
	wlr_scene_node_destroy(&view->image_capture_scene->tree.node);
	wlr_scene_node_destroy(&view->scene_tree->node);
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
#if WLR_HAS_XWAYLAND
uint32_t view_get_x11_window_id(struct sway_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_X11_WINDOW_ID);
	}
	return 0;
}

uint32_t view_get_x11_parent_id(struct sway_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_X11_PARENT_ID);
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

static const struct wlr_security_context_v1_state *security_context_from_view(
		struct sway_view *view) {
	const struct wl_client *client =
		wl_resource_get_client(view->surface->resource);
	const struct wlr_security_context_v1_state *security_context =
		wlr_security_context_manager_v1_lookup_client(
				server.security_context_manager_v1, client);
	return security_context;
}

const char *view_get_sandbox_engine(struct sway_view *view) {
	const struct wlr_security_context_v1_state *security_context =
		security_context_from_view(view);
	return security_context ? security_context->sandbox_engine : NULL;
}

const char *view_get_sandbox_app_id(struct sway_view *view) {
	const struct wlr_security_context_v1_state *security_context =
		security_context_from_view(view);
	return security_context ? security_context->app_id : NULL;
}

const char *view_get_sandbox_instance_id(struct sway_view *view) {
	const struct wlr_security_context_v1_state *security_context =
		security_context_from_view(view);
	return security_context ? security_context->instance_id : NULL;
}

const char *view_get_tag(struct sway_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_TAG);
	}
	return NULL;
}

const char *view_get_shell(struct sway_view *view) {
	switch(view->type) {
	case SWAY_VIEW_XDG_SHELL:
		return "xdg_shell";
#if WLR_HAS_XWAYLAND
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
		*min_width = 1;
		*max_width = DBL_MAX;
		*min_height = 1;
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

bool view_inhibit_idle(struct sway_view *view) {
	if (server.session_lock.lock) {
		return false;
	}

	struct sway_idle_inhibitor_v1 *user_inhibitor =
		sway_idle_inhibit_v1_user_inhibitor_for_view(view);

	struct sway_idle_inhibitor_v1 *application_inhibitor =
		sway_idle_inhibit_v1_application_inhibitor_for_view(view);

	if (!user_inhibitor && !application_inhibitor) {
		return false;
	}

	if (!user_inhibitor) {
		return sway_idle_inhibit_v1_is_active(application_inhibitor);
	}

	if (!application_inhibitor) {
		return sway_idle_inhibit_v1_is_active(user_inhibitor);
	}

	return sway_idle_inhibit_v1_is_active(user_inhibitor)
		|| sway_idle_inhibit_v1_is_active(application_inhibitor);
}

bool view_ancestor_is_only_visible(struct sway_view *view) {
	bool only_visible = true;
	struct sway_container *con = view->container;
	while (con) {
		enum sway_container_layout layout = container_parent_layout(con);
		if (layout != L_TABBED && layout != L_STACKED) {
			list_t *siblings = container_get_siblings(con);
			if (siblings && siblings->length > 1) {
				only_visible = false;
			}
		} else {
			only_visible = true;
		}
		con = con->pending.parent;
	}
	return only_visible;
}

static bool view_is_only_visible(struct sway_view *view) {
	struct sway_container *con = view->container;
	while (con) {
		enum sway_container_layout layout = container_parent_layout(con);
		if (layout != L_TABBED && layout != L_STACKED) {
			list_t *siblings = container_get_siblings(con);
			if (siblings && siblings->length > 1) {
				return false;
			}
		}

		con = con->pending.parent;
	}

	return true;
}

static bool gaps_to_edge(struct sway_view *view) {
	struct side_gaps gaps = view->container->pending.workspace->current_gaps;
	return gaps.top > 0 || gaps.right > 0 || gaps.bottom > 0 || gaps.left > 0;
}

void view_autoconfigure(struct sway_view *view) {
	struct sway_container *con = view->container;
	struct sway_workspace *ws = con->pending.workspace;

	if (container_is_scratchpad_hidden(con) &&
			con->pending.fullscreen_mode != FULLSCREEN_GLOBAL) {
		return;
	}
	struct sway_output *output = ws ? ws->output : NULL;

	if (con->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
		con->pending.content_x = output->lx;
		con->pending.content_y = output->ly;
		con->pending.content_width = output->width;
		con->pending.content_height = output->height;
		return;
	} else if (con->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		con->pending.content_x = root->x;
		con->pending.content_y = root->y;
		con->pending.content_width = root->width;
		con->pending.content_height = root->height;
		return;
	}

	con->pending.border_top = con->pending.border_bottom = true;
	con->pending.border_left = con->pending.border_right = true;
	double y_offset = 0;

	if (!container_is_floating_or_child(con) && ws) {
		if (config->hide_edge_borders == E_BOTH
				|| config->hide_edge_borders == E_VERTICAL) {
			con->pending.border_left = con->pending.x != ws->x;
			int right_x = con->pending.x + con->pending.width;
			con->pending.border_right = right_x != ws->x + ws->width;
		}

		if (config->hide_edge_borders == E_BOTH
				|| config->hide_edge_borders == E_HORIZONTAL) {
			con->pending.border_top = con->pending.y != ws->y;
			int bottom_y = con->pending.y + con->pending.height;
			con->pending.border_bottom = bottom_y != ws->y + ws->height;
		}

		bool smart = config->hide_edge_borders_smart == ESMART_ON ||
			(config->hide_edge_borders_smart == ESMART_NO_GAPS &&
			!gaps_to_edge(view));
		if (smart) {
			bool show_border = !view_is_only_visible(view);
			con->pending.border_left &= show_border;
			con->pending.border_right &= show_border;
			con->pending.border_top &= show_border;
			con->pending.border_bottom &= show_border;
		}
	}

	if (!container_is_floating(con)) {
		// In a tabbed or stacked container, the container's y is the top of the
		// title area. We have to offset the surface y by the height of the title,
		// bar, and disable any top border because we'll always have the title bar.
		list_t *siblings = container_get_siblings(con);
		bool show_titlebar = (siblings && siblings->length > 1)
			|| !config->hide_lone_tab;
		if (show_titlebar) {
			enum sway_container_layout layout = container_parent_layout(con);
			if (layout == L_TABBED) {
				y_offset = container_titlebar_height();
				con->pending.border_top = false;
			} else if (layout == L_STACKED) {
				y_offset = container_titlebar_height() * siblings->length;
				con->pending.border_top = false;
			}
		}
	}

	double x, y, width, height;
	switch (con->pending.border) {
	default:
	case B_CSD:
	case B_NONE:
		x = con->pending.x;
		y = con->pending.y + y_offset;
		width = con->pending.width;
		height = con->pending.height - y_offset;
		break;
	case B_PIXEL:
		x = con->pending.x + con->pending.border_thickness * con->pending.border_left;
		y = con->pending.y + con->pending.border_thickness * con->pending.border_top + y_offset;
		width = con->pending.width
			- con->pending.border_thickness * con->pending.border_left
			- con->pending.border_thickness * con->pending.border_right;
		height = con->pending.height - y_offset
			- con->pending.border_thickness * con->pending.border_top
			- con->pending.border_thickness * con->pending.border_bottom;
		break;
	case B_NORMAL:
		// Height is: 1px border + 3px pad + title height + 3px pad + 1px border
		x = con->pending.x + con->pending.border_thickness * con->pending.border_left;
		width = con->pending.width
			- con->pending.border_thickness * con->pending.border_left
			- con->pending.border_thickness * con->pending.border_right;
		if (y_offset) {
			y = con->pending.y + y_offset;
			height = con->pending.height - y_offset
				- con->pending.border_thickness * con->pending.border_bottom;
		} else {
			y = con->pending.y + container_titlebar_height();
			height = con->pending.height - container_titlebar_height()
				- con->pending.border_thickness * con->pending.border_bottom;
		}
		break;
	}

	con->pending.content_x = x;
	con->pending.content_y = y;
	con->pending.content_width = fmax(width, 1);
	con->pending.content_height = fmax(height, 1);
}

void view_set_activated(struct sway_view *view, bool activated) {
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
	if (view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(
				view->foreign_toplevel, activated);
	}
}

void view_request_activate(struct sway_view *view, struct sway_seat *seat) {
	struct sway_workspace *ws = view->container->pending.workspace;
	if (!seat) {
		seat = input_manager_current_seat();
	}

	switch (config->focus_on_window_activation) {
	case FOWA_SMART:
		if (ws && workspace_is_visible(ws)) {
			seat_set_focus_container(seat, view->container);
			container_raise_floating(view->container);
		} else {
			view_set_urgent(view, true);
		}
		break;
	case FOWA_URGENT:
		view_set_urgent(view, true);
		break;
	case FOWA_FOCUS:
		if (container_is_scratchpad_hidden_or_child(view->container)) {
			root_scratchpad_show(view->container);
		} else {
			seat_set_focus_container(seat, view->container);
			container_raise_floating(view->container);
		}
		break;
	case FOWA_NONE:
		break;
	}
	transaction_commit_dirty();
}

void view_request_urgent(struct sway_view *view) {
	if (config->focus_on_window_activation != FOWA_NONE) {
		view_set_urgent(view, true);
	}
}

void view_set_csd_from_server(struct sway_view *view, bool enabled) {
	sway_log(SWAY_DEBUG, "Telling view %p to set CSD to %i", view, enabled);
	if (view->xdg_decoration) {
		uint32_t mode = enabled ?
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE :
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
		wlr_xdg_toplevel_decoration_v1_set_mode(
				view->xdg_decoration->wlr_xdg_decoration, mode);
	}
	view->using_csd = enabled;
}

void view_update_csd_from_client(struct sway_view *view, bool enabled) {
	sway_log(SWAY_DEBUG, "View %p updated CSD to %i", view, enabled);
	struct sway_container *con = view->container;
	if (enabled && con && con->pending.border != B_CSD) {
		con->saved_border = con->pending.border;
		if (container_is_floating(con)) {
			con->pending.border = B_CSD;
		}
	} else if (!enabled && con && con->pending.border == B_CSD) {
		con->pending.border = con->saved_border;
	}
	view->using_csd = enabled;
}

void view_set_tiled(struct sway_view *view, bool tiled) {
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
	list_t *criterias = criteria_for_view(view, CT_COMMAND);
	for (int i = 0; i < criterias->length; i++) {
		struct criteria *criteria = criterias->items[i];
		sway_log(SWAY_DEBUG, "Checking criteria %s", criteria->raw);
		if (view_has_executed_criteria(view, criteria)) {
			sway_log(SWAY_DEBUG, "Criteria already executed");
			continue;
		}
		sway_log(SWAY_DEBUG, "for_window '%s' matches view %p, cmd: '%s'",
				criteria->raw, view, criteria->cmdlist);
		list_add(view->executed_criteria, criteria);
		list_t *res_list = execute_command(criteria->cmdlist, NULL, view->container);
		while (res_list->length) {
			struct cmd_results *res = res_list->items[0];
			if (res->status != CMD_SUCCESS) {
				sway_log(SWAY_ERROR, "for_window '%s' failed: %s", criteria->raw, res->error);
			}
			free_cmd_results(res);
			list_del(res_list, 0);
		}
		list_free(res_list);
	}
	list_free(criterias);
}

static void view_populate_pid(struct sway_view *view) {
	pid_t pid;
	switch (view->type) {
#if WLR_HAS_XWAYLAND
	case SWAY_VIEW_XWAYLAND:;
		struct wlr_xwayland_surface *surf =
			wlr_xwayland_surface_try_from_wlr_surface(view->surface);
		pid = surf->pid;
		break;
#endif
	case SWAY_VIEW_XDG_SHELL:;
		struct wl_client *client =
			wl_resource_get_client(view->surface->resource);
		wl_client_get_credentials(client, &pid, NULL, NULL);
		break;
	}
	view->pid = pid;
}

void view_assign_ctx(struct sway_view *view, struct launcher_ctx *ctx) {
	if (view->ctx) {
		// This ctx has been replaced
		launcher_ctx_destroy(view->ctx);
		view->ctx = NULL;
	}
	if (ctx == NULL) {
		return;
	}
	launcher_ctx_consume(ctx);

	view->ctx = ctx;
}

static struct sway_workspace *select_workspace(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat();

	// Check if there's any `assign` criteria for the view
	list_t *criterias = criteria_for_view(view,
			CT_ASSIGN_WORKSPACE | CT_ASSIGN_WORKSPACE_NUMBER | CT_ASSIGN_OUTPUT);
	struct sway_workspace *ws = NULL;
	for (int i = 0; i < criterias->length; ++i) {
		struct criteria *criteria = criterias->items[i];
		if (criteria->type == CT_ASSIGN_OUTPUT) {
			struct sway_output *output = output_by_name_or_id(criteria->target);
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
					if (seat->prev_workspace_name) {
						ws = workspace_create(NULL, seat->prev_workspace_name);
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
		view_assign_ctx(view, NULL);
		return ws;
	}

	// Check if there's a PID mapping
	ws = view->ctx ? launcher_ctx_get_workspace(view->ctx) : NULL;
	if (ws) {
		view_assign_ctx(view, NULL);
		return ws;
	}

	// Use the focused workspace
	struct sway_node *node = seat_get_focus_inactive(seat, &root->node);
	if (node && node->type == N_WORKSPACE) {
		return node->sway_workspace;
	} else if (node && node->type == N_CONTAINER) {
		return node->sway_container->pending.workspace;
	}

	// When there's no outputs connected, the above should match a workspace on
	// the noop output.
	sway_assert(false, "Expected to find a workspace");
	return NULL;
}

static void update_ext_foreign_toplevel(struct sway_view *view) {
	struct wlr_ext_foreign_toplevel_handle_v1_state toplevel_state = {
		.app_id = view_get_app_id(view),
		.title = view_get_title(view),
	};
	wlr_ext_foreign_toplevel_handle_v1_update_state(view->ext_foreign_toplevel, &toplevel_state);
}

static bool should_focus(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *prev_con = seat_get_focused_container(seat);
	struct sway_workspace *prev_ws = seat_get_focused_workspace(seat);
	struct sway_workspace *map_ws = view->container->pending.workspace;

	if (view->container->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		return true;
	}

	// View opened "under" fullscreen view should not be given focus.
	if (root->fullscreen_global || !map_ws || map_ws->fullscreen) {
		return false;
	}

	// Views can only take focus if they are mapped into the active workspace
	if (prev_ws != map_ws) {
		return false;
	}

	// If the view is the only one in the focused workspace, it'll get focus
	// regardless of any no_focus criteria.
	if (!view->container->pending.parent && !prev_con) {
		size_t num_children = view->container->pending.workspace->tiling->length +
			view->container->pending.workspace->floating->length;
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

static void handle_foreign_activate_request(
		struct wl_listener *listener, void *data) {
	struct sway_view *view = wl_container_of(
			listener, view, foreign_activate_request);
	struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		if (seat->wlr_seat == event->seat) {
			if (container_is_scratchpad_hidden_or_child(view->container)) {
				root_scratchpad_show(view->container);
			}
			seat_set_focus_container(seat, view->container);
			seat_consider_warp_to_focus(seat);
			container_raise_floating(view->container);
			break;
		}
	}
	transaction_commit_dirty();
}

static void handle_foreign_fullscreen_request(
		struct wl_listener *listener, void *data) {
	struct sway_view *view = wl_container_of(
			listener, view, foreign_fullscreen_request);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

	// Match fullscreen command behavior for scratchpad hidden views
	struct sway_container *container = view->container;
	if (!container->pending.workspace) {
		while (container->pending.parent) {
			container = container->pending.parent;
		}
	}

	if (event->fullscreen && event->output && event->output->data) {
		struct sway_output *output = event->output->data;
		struct sway_workspace *ws = output_get_active_workspace(output);
		if (ws && !container_is_scratchpad_hidden(view->container)) {
			if (container_is_floating(view->container)) {
				workspace_add_floating(ws, view->container);
			} else {
				workspace_add_tiling(ws, view->container);
			}
		}
	}

	container_set_fullscreen(container,
		event->fullscreen ? FULLSCREEN_WORKSPACE : FULLSCREEN_NONE);
	if (event->fullscreen) {
		arrange_root();
	} else {
		if (container->pending.parent) {
			arrange_container(container->pending.parent);
		} else if (container->pending.workspace) {
			arrange_workspace(container->pending.workspace);
		}
	}
	transaction_commit_dirty();
}

static void handle_foreign_close_request(
		struct wl_listener *listener, void *data) {
	struct sway_view *view = wl_container_of(
			listener, view, foreign_close_request);
	view_close(view);
}

static void handle_foreign_destroy(
		struct wl_listener *listener, void *data) {
	struct sway_view *view = wl_container_of(
			listener, view, foreign_destroy);

	wl_list_remove(&view->foreign_activate_request.link);
	wl_list_remove(&view->foreign_fullscreen_request.link);
	wl_list_remove(&view->foreign_close_request.link);
	wl_list_remove(&view->foreign_destroy.link);
}

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface,
			  bool fullscreen, struct wlr_output *fullscreen_output,
			  bool decoration) {
	if (!sway_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}
	view->surface = wlr_surface;
	view_populate_pid(view);
	view->container = container_create(view);

	if (view->ctx == NULL) {
		struct launcher_ctx *ctx = launcher_ctx_find_pid(view->pid);
		if (ctx != NULL) {
			view_assign_ctx(view, ctx);
		}
	}

	// If there is a request to be opened fullscreen on a specific output, try
	// to honor that request. Otherwise, fallback to assigns, pid mappings,
	// focused workspace, etc
	struct sway_workspace *ws = NULL;
	if (fullscreen_output && fullscreen_output->data) {
		struct sway_output *output = fullscreen_output->data;
		ws = output_get_active_workspace(output);
	}
	if (!ws) {
		ws = select_workspace(view);
	}

	if (ws && ws->output) {
		// Once the output is determined, we can notify the client early about
		// scale to reduce startup jitter.
		float scale = ws->output->wlr_output->scale;
		wlr_fractional_scale_v1_notify_scale(wlr_surface, scale);
		wlr_surface_set_preferred_buffer_scale(wlr_surface, ceil(scale));
	}

	struct sway_seat *seat = input_manager_current_seat();
	struct sway_node *node =
		seat_get_focus_inactive(seat, ws ? &ws->node : &root->node);
	struct sway_container *target_sibling = NULL;
	if (node && node->type == N_CONTAINER) {
		if (container_is_floating(node->sway_container)) {
			// If we're about to launch the view into the floating container, then
			// launch it as a tiled view instead.
			if (ws) {
				target_sibling = seat_get_focus_inactive_tiling(seat, ws);
				if (target_sibling) {
					struct sway_container *con =
						seat_get_focus_inactive_view(seat, &target_sibling->node);
					if (con)  {
						target_sibling = con;
					}
				}
			} else {
				ws = seat_get_last_known_workspace(seat);
			}
		} else {
			target_sibling = node->sway_container;
		}
	}

	struct wlr_ext_foreign_toplevel_handle_v1_state foreign_toplevel_state = {
		.app_id = view_get_app_id(view),
		.title = view_get_title(view),
	};
	view->ext_foreign_toplevel =
		wlr_ext_foreign_toplevel_handle_v1_create(server.foreign_toplevel_list, &foreign_toplevel_state);
	view->ext_foreign_toplevel->data = view;

	view->foreign_toplevel =
		wlr_foreign_toplevel_handle_v1_create(server.foreign_toplevel_manager);
	view->foreign_activate_request.notify = handle_foreign_activate_request;
	wl_signal_add(&view->foreign_toplevel->events.request_activate,
			&view->foreign_activate_request);
	view->foreign_fullscreen_request.notify = handle_foreign_fullscreen_request;
	wl_signal_add(&view->foreign_toplevel->events.request_fullscreen,
			&view->foreign_fullscreen_request);
	view->foreign_close_request.notify = handle_foreign_close_request;
	wl_signal_add(&view->foreign_toplevel->events.request_close,
			&view->foreign_close_request);
	view->foreign_destroy.notify = handle_foreign_destroy;
	wl_signal_add(&view->foreign_toplevel->events.destroy,
			&view->foreign_destroy);

	struct sway_container *container = view->container;
	if (target_sibling) {
		container_add_sibling(target_sibling, container, 1);
	} else if (ws) {
		container = workspace_add_tiling(ws, container);
	}
	ipc_event_window(view->container, "new");

	if (decoration) {
		view_update_csd_from_client(view, decoration);
	}

	if (view->impl->wants_floating && view->impl->wants_floating(view)) {
		view->container->pending.border = config->floating_border;
		view->container->pending.border_thickness = config->floating_border_thickness;
		container_set_floating(view->container, true);
	} else {
		view->container->pending.border = config->border;
		view->container->pending.border_thickness = config->border_thickness;
		view_set_tiled(view, true);
	}

	if (config->popup_during_fullscreen == POPUP_LEAVE &&
			container->pending.workspace &&
			container->pending.workspace->fullscreen &&
			container->pending.workspace->fullscreen->view) {
		struct sway_container *fs = container->pending.workspace->fullscreen;
		if (view_is_transient_for(view, fs->view)) {
			container_set_fullscreen(fs, false);
		}
	}

	view_update_title(view, false);
	container_update_representation(container);

	if (fullscreen) {
		container_set_fullscreen(view->container, true);
		arrange_workspace(view->container->pending.workspace);
	} else {
		if (container->pending.parent) {
			arrange_container(container->pending.parent);
		} else if (container->pending.workspace) {
			arrange_workspace(container->pending.workspace);
		}
	}

	view_execute_criteria(view);

	bool set_focus = should_focus(view);

#if WLR_HAS_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(wlr_surface))) {
		set_focus &= wlr_xwayland_surface_icccm_input_model(xsurface) !=
				WLR_ICCCM_INPUT_MODEL_NONE;
	}
#endif

	if (set_focus) {
		input_manager_set_focus(&view->container->node);
	}

	if (view->ext_foreign_toplevel) {
		update_ext_foreign_toplevel(view);
	}

	const char *app_id;
	const char *class;
	if ((app_id = view_get_app_id(view)) != NULL) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);
	} else if ((class = view_get_class(view)) != NULL) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, class);
	}
}

void view_unmap(struct sway_view *view) {
	wl_signal_emit_mutable(&view->events.unmap, view);

	view->executed_criteria->length = 0;

	if (view->urgent_timer) {
		wl_event_source_remove(view->urgent_timer);
		view->urgent_timer = NULL;
	}

	if (view->ext_foreign_toplevel) {
		wlr_ext_foreign_toplevel_handle_v1_destroy(view->ext_foreign_toplevel);
		view->ext_foreign_toplevel = NULL;
	}

	if (view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(view->foreign_toplevel);
		view->foreign_toplevel = NULL;
	}

	struct sway_container *parent = view->container->pending.parent;
	struct sway_workspace *ws = view->container->pending.workspace;
	container_begin_destroy(view->container);
	if (parent) {
		container_reap_empty(parent);
	} else if (ws) {
		workspace_consider_destroy(ws);
	}

	if (root->fullscreen_global) {
		// Container may have been a child of the root fullscreen container
		arrange_root();
	} else if (ws && !ws->node.destroying) {
		arrange_workspace(ws);
		workspace_detect_urgent(ws);
	}

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat->cursor->image_surface = NULL;
		if (seat->cursor->active_constraint) {
			struct wlr_surface *constrain_surface =
				seat->cursor->active_constraint->surface;
			if (view_from_wlr_surface(constrain_surface) == view) {
				sway_cursor_constrain(seat->cursor, NULL);
			}
		}
		seat_consider_warp_to_focus(seat);
	}

	transaction_commit_dirty();
	view->surface = NULL;
}

void view_update_size(struct sway_view *view) {
	struct sway_container *con = view->container;
	con->pending.content_width = view->geometry.width;
	con->pending.content_height = view->geometry.height;
	container_set_geometry_from_content(con);
}

void view_center_and_clip_surface(struct sway_view *view) {
	struct sway_container *con = view->container;

	bool clip_to_geometry = true;

	if (container_is_floating(con)) {
		// We always center the current coordinates rather than the next, as the
		// geometry immediately affects the currently active rendering.
		int x = (int) fmax(0, (con->current.content_width - view->geometry.width) / 2);
		int y = (int) fmax(0, (con->current.content_height - view->geometry.height) / 2);
		clip_to_geometry = !view->using_csd;

		wlr_scene_node_set_position(&view->content_tree->node, x, y);
	} else {
		wlr_scene_node_set_position(&view->content_tree->node, 0, 0);
	}

	// only make sure to clip the content if there is content to clip
	if (!wl_list_empty(&con->view->content_tree->children)) {
		struct wlr_box clip = {0};
		if (clip_to_geometry) {
			clip = (struct wlr_box){
				.x = con->view->geometry.x,
				.y = con->view->geometry.y,
				.width = con->current.content_width,
				.height = con->current.content_height,
			};
		}
		wlr_scene_subsurface_tree_set_clip(&con->view->content_tree->node, &clip);
	}
}

struct sway_view *view_from_wlr_surface(struct wlr_surface *wlr_surface) {
	struct wlr_xdg_surface *xdg_surface;
	if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(wlr_surface))) {
		return view_from_wlr_xdg_surface(xdg_surface);
	}
#if WLR_HAS_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(wlr_surface))) {
		return view_from_wlr_xwayland_surface(xsurface);
	}
#endif
	struct wlr_subsurface *subsurface;
	if ((subsurface = wlr_subsurface_try_from_wlr_surface(wlr_surface))) {
		return view_from_wlr_surface(subsurface->parent);
	}
	if (wlr_layer_surface_v1_try_from_wlr_surface(wlr_surface) != NULL) {
		return NULL;
	}
	if (wlr_session_lock_surface_v1_try_from_wlr_surface(wlr_surface) != NULL) {
		return NULL;
	}

	const char *role = wlr_surface->role ? wlr_surface->role->name : NULL;
	sway_log(SWAY_DEBUG, "Surface of unknown type (role %s): %p",
		role, wlr_surface);
	return NULL;
}

void view_update_app_id(struct sway_view *view) {
	const char *app_id = view_get_app_id(view);

	if (view->foreign_toplevel && app_id) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);
	}

	if (view->ext_foreign_toplevel) {
		update_ext_foreign_toplevel(view);
	}
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

	size_t len = parse_title_format(view->container, NULL);

	if (len) {
		char *buffer = calloc(len + 1, sizeof(char));
		if (!sway_assert(buffer, "Unable to allocate title string")) {
			return;
		}

		parse_title_format(view->container, buffer);
		view->container->formatted_title = buffer;
	} else {
		view->container->formatted_title = NULL;
	}

	view->container->title = title ? strdup(title) : NULL;

	// Update title after the global font height is updated
	if (view->container->title_bar.title_text && len) {
		sway_text_node_set_text(view->container->title_bar.title_text,
			view->container->formatted_title);
		container_arrange_title_bar(view->container);
	} else {
		container_update_title_bar(view->container);
	}

	ipc_event_window(view->container, "title");

	if (view->foreign_toplevel && title) {
		wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel, title);
	}

	if (view->ext_foreign_toplevel) {
		update_ext_foreign_toplevel(view);
	}
}

bool view_is_visible(struct sway_view *view) {
	if (view->container->node.destroying) {
		return false;
	}
	struct sway_workspace *workspace = view->container->pending.workspace;
	if (!workspace && view->container->pending.fullscreen_mode != FULLSCREEN_GLOBAL) {
		bool fs_global_descendant = false;
		struct sway_container *parent = view->container->pending.parent;
		while (parent) {
			if (parent->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
				fs_global_descendant = true;
			}
			parent = parent->pending.parent;
		}
		if (!fs_global_descendant) {
			return false;
		}
	}

	if (!container_is_sticky_or_child(view->container) && workspace &&
			!workspace_is_visible(workspace)) {
		return false;
	}
	// Check view isn't in a tabbed or stacked container on an inactive tab
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *con = view->container;
	while (con) {
		enum sway_container_layout layout = container_parent_layout(con);
		if ((layout == L_TABBED || layout == L_STACKED)
				&& !container_is_floating(con)) {
			struct sway_node *parent = con->pending.parent ?
				&con->pending.parent->node : &con->pending.workspace->node;
			if (seat_get_active_tiling_child(seat, parent) != &con->node) {
				return false;
			}
		}
		con = con->pending.parent;
	}
	// Check view isn't hidden by another fullscreen view
	struct sway_container *fs = root->fullscreen_global ?
		root->fullscreen_global : workspace->fullscreen;
	if (fs && !container_is_fullscreen_or_child(view->container) &&
			!container_is_transient_for(view->container, fs)) {
		return false;
	}
	return true;
}

void view_set_urgent(struct sway_view *view, bool enable) {
	if (view_is_urgent(view) == enable) {
		return;
	}
	if (enable) {
		struct sway_seat *seat = input_manager_current_seat();
		if (seat_get_focused_container(seat) == view->container) {
			return;
		}
		clock_gettime(CLOCK_MONOTONIC, &view->urgent);
		container_update_itself_and_parents(view->container);
	} else {
		view->urgent = (struct timespec){ 0 };
		if (view->urgent_timer) {
			wl_event_source_remove(view->urgent_timer);
			view->urgent_timer = NULL;
		}
	}

	ipc_event_window(view->container, "urgent");

	if (!container_is_scratchpad_hidden_or_child(view->container)) {
		workspace_detect_urgent(view->container->pending.workspace);
	}
}

bool view_is_urgent(struct sway_view *view) {
	return view->urgent.tv_sec || view->urgent.tv_nsec;
}

void view_remove_saved_buffer(struct sway_view *view) {
	if (!sway_assert(view->saved_surface_tree, "Expected a saved buffer")) {
		return;
	}

	wlr_scene_node_destroy(&view->saved_surface_tree->node);
	view->saved_surface_tree = NULL;
	wlr_scene_node_set_enabled(&view->content_tree->node, true);
}

static void view_save_buffer_iterator(struct wlr_scene_buffer *buffer,
		double sx, double sy, void *data) {
	struct wlr_scene_tree *tree = data;

	struct wlr_scene_buffer *sbuf = wlr_scene_buffer_create(tree, NULL);
	if (!sbuf) {
		sway_log(SWAY_ERROR, "Could not allocate a scene buffer when saving a surface");
		return;
	}

	wlr_scene_buffer_set_dest_size(sbuf,
		buffer->dst_width, buffer->dst_height);
	wlr_scene_buffer_set_opaque_region(sbuf, &buffer->opaque_region);
	wlr_scene_buffer_set_opacity(sbuf, buffer->opacity);
	wlr_scene_buffer_set_filter_mode(sbuf, buffer->filter_mode);
	wlr_scene_buffer_set_transfer_function(sbuf, buffer->transfer_function);
	wlr_scene_buffer_set_primaries(sbuf, buffer->primaries);
	wlr_scene_buffer_set_source_box(sbuf, &buffer->src_box);
	wlr_scene_node_set_position(&sbuf->node, sx, sy);
	wlr_scene_buffer_set_transform(sbuf, buffer->transform);
	wlr_scene_buffer_set_buffer(sbuf, buffer->buffer);
}

void view_save_buffer(struct sway_view *view) {
	if (!sway_assert(!view->saved_surface_tree, "Didn't expect saved buffer")) {
		view_remove_saved_buffer(view);
	}

	view->saved_surface_tree = wlr_scene_tree_create(view->scene_tree);
	if (!view->saved_surface_tree) {
		sway_log(SWAY_ERROR, "Could not allocate a scene tree node when saving a surface");
		return;
	}

	// Enable and disable the saved surface tree like so to atomitaclly update
	// the tree. This will prevent over damaging or other weirdness.
	wlr_scene_node_set_enabled(&view->saved_surface_tree->node, false);

	wlr_scene_node_for_each_buffer(&view->content_tree->node,
		view_save_buffer_iterator, view->saved_surface_tree);

	wlr_scene_node_set_enabled(&view->content_tree->node, false);
	wlr_scene_node_set_enabled(&view->saved_surface_tree->node, true);
}

bool view_is_transient_for(struct sway_view *child,
		struct sway_view *ancestor) {
	return child->impl->is_transient_for &&
		child->impl->is_transient_for(child, ancestor);
}

bool view_can_tear(struct sway_view *view) {
	switch (view->tearing_mode) {
	case TEARING_OVERRIDE_FALSE:
		return false;
	case TEARING_OVERRIDE_TRUE:
		return true;
	case TEARING_WINDOW_HINT:
		return view->tearing_hint ==
			WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC;
	}
	return false;
}

static void send_frame_done_iterator(struct wlr_scene_buffer *scene_buffer,
		double x, double y, void *data) {
	struct timespec *when = data;
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
	if (scene_surface == NULL) {
		return;
	}
	wlr_surface_send_frame_done(scene_surface->surface, when);
}

void view_send_frame_done(struct sway_view *view) {
	struct timespec when;
	clock_gettime(CLOCK_MONOTONIC, &when);

	struct wlr_scene_node *node;
	wl_list_for_each(node, &view->content_tree->children, link) {
		wlr_scene_node_for_each_buffer(node, send_frame_done_iterator, &when);
	}
}
