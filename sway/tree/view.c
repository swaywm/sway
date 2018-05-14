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

void view_init(struct sway_view *view, enum sway_view_type type,
		const struct sway_view_impl *impl) {
	view->type = type;
	view->impl = impl;
	view->executed_criteria = create_list();
	wl_signal_init(&view->events.unmap);
}

void view_destroy(struct sway_view *view) {
	if (view == NULL) {
		return;
	}

	if (view->surface != NULL) {
		view_unmap(view);
	}

	list_free(view->executed_criteria);

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

const char *view_get_type(struct sway_view *view) {
	switch(view->type) {
	case SWAY_VIEW_WL_SHELL:
		return "wl_shell";
	case SWAY_VIEW_XDG_SHELL_V6:
		return "xdg_shell_v6";
	case SWAY_VIEW_XDG_SHELL:
		return "xdg_shell";
	case SWAY_VIEW_XWAYLAND:
		return "xwayland";
	}
	return "unknown";
}

void view_configure(struct sway_view *view, double ox, double oy, int width,
		int height) {
	if (view->impl->configure) {
		view->impl->configure(view, ox, oy, width, height);
	}
}

void view_autoconfigure(struct sway_view *view) {
	if (!sway_assert(view->swayc,
				"Called view_autoconfigure() on a view without a swayc")) {
		return;
	}

	if (view->is_fullscreen) {
		struct sway_container *output = container_parent(view->swayc, C_OUTPUT);
		view_configure(view, 0, 0, output->width, output->height);
		view->x = view->y = 0;
		return;
	}

	int other_views = 1;
	if (config->hide_edge_borders == E_SMART) {
		struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);
		other_views = container_count_descendants_of_type(ws, C_VIEW) - 1;
	}

	double x, y, width, height;
	x = y = width = height = 0;
	switch (view->border) {
	case B_NONE:
		x = view->swayc->x;
		y = view->swayc->y;
		width = view->swayc->width;
		height = view->swayc->height;
		break;
	case B_PIXEL:
		if (view->swayc->layout > L_VERT
				|| config->hide_edge_borders == E_NONE
				|| config->hide_edge_borders == E_HORIZONTAL
				|| (config->hide_edge_borders == E_SMART && other_views)) {
			x = view->swayc->x + view->border_thickness;
			width = view->swayc->width - view->border_thickness * 2;
		} else {
			x = view->swayc->x;
			width = view->swayc->width;
		}
		if (view->swayc->layout > L_VERT
				|| config->hide_edge_borders == E_NONE
				|| config->hide_edge_borders == E_VERTICAL
				|| (config->hide_edge_borders == E_SMART && other_views)) {
			y = view->swayc->y + view->border_thickness;
			height = view->swayc->height - view->border_thickness * 2;
		} else {
			y = view->swayc->y;
			height = view->swayc->height;
		}
		break;
	case B_NORMAL:
		if (view->swayc->layout > L_VERT
				|| config->hide_edge_borders == E_NONE
				|| config->hide_edge_borders == E_HORIZONTAL
				|| (config->hide_edge_borders == E_SMART && other_views)) {
			x = view->swayc->x + view->border_thickness;
			width = view->swayc->width - view->border_thickness * 2;
		} else {
			x = view->swayc->x;
			width = view->swayc->width;
		}
		if (view->swayc->layout > L_VERT
				|| config->hide_edge_borders == E_NONE
				|| config->hide_edge_borders == E_VERTICAL
				|| (config->hide_edge_borders == E_SMART && other_views)) {
			// Height is: border + title height + border + view height + border
			y = view->swayc->y + config->font_height
				+ view->border_thickness * 2;
			height = view->swayc->height - config->font_height
				- view->border_thickness * 3;
		} else {
			y = view->swayc->y;
			height = view->swayc->height;
		}
		break;
	}

	view->x = x;
	view->y = y;
	view_configure(view, x, y, width, height);
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
		view->swayc->width = view->swayc->saved_width;
		view->swayc->height = view->swayc->saved_height;
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
	struct sway_container *prior_workspace =
		container_parent(view->swayc, C_WORKSPACE);
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
	seat_set_focus(seat, seat_get_focus_inactive(seat, prior_workspace));
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
	if (criterias->length) {
		struct criteria *criteria = criterias->items[0];
		if (criteria->type == CT_ASSIGN_WORKSPACE) {
			struct sway_container *workspace = workspace_by_name(criteria->target);
			if (!workspace) {
				workspace = workspace_create(NULL, criteria->target);
			}
			focus = seat_get_focus_inactive(seat, workspace);
		} else {
			// TODO: CT_ASSIGN_OUTPUT
		}
	}
	free(criterias);
	cont = container_view_create(focus, view);

	view->surface = wlr_surface;
	view->swayc = cont;
	view->border = config->border;
	view->border_thickness = config->border_thickness;

	view_init_subsurfaces(view, wlr_surface);
	wl_signal_add(&wlr_surface->events.new_subsurface,
		&view->surface_new_subsurface);
	view->surface_new_subsurface.notify = view_handle_surface_new_subsurface;

	wl_signal_add(&view->swayc->events.reparent, &view->container_reparent);
	view->container_reparent.notify = view_handle_container_reparent;

	arrange_children_of(cont->parent);
	input_manager_set_focus(input_manager, cont);

	view_update_title(view, false);
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

void view_update_position(struct sway_view *view, double ox, double oy) {
	if (view->swayc->x == ox && view->swayc->y == oy) {
		return;
	}

	// TODO: Only allow this if the view is floating (this function will only be
	// called in response to wayland clients wanting to reposition themselves).
	container_damage_whole(view->swayc);
	view->swayc->x = ox;
	view->swayc->y = oy;
	container_damage_whole(view->swayc);
}

void view_update_size(struct sway_view *view, int width, int height) {
	if (view->width == width && view->height == height) {
		return;
	}

	container_damage_whole(view->swayc);
	// Should we update the swayc width/height here too?
	view->width = width;
	view->height = height;
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
	const char *title = view_get_title(view);
	const char *class = view_get_class(view);
	const char *instance = view_get_instance(view);
	const char *shell = view_get_type(view);
	size_t title_len = title ? strlen(title) : 0;
	size_t class_len = class ? strlen(class) : 0;
	size_t instance_len = instance ? strlen(instance) : 0;
	size_t shell_len = shell ? strlen(shell) : 0;

	size_t len = 0;
	char *format = view->title_format;
	char *next = strchr(format, '%');
	while (next) {
		if (buffer) {
			// Copy everything up to the %
			strncat(buffer, format, next - format);
		}
		len += next - format;
		format = next;

		if (strncmp(next, "%title", 6) == 0) {
			if (buffer && title) {
				strcat(buffer, title);
			}
			len += title_len;
			format += 6;
		} else if (strncmp(next, "%class", 6) == 0) {
			if (buffer && class) {
				strcat(buffer, class);
			}
			len += class_len;
			format += 6;
		} else if (strncmp(next, "%instance", 9) == 0) {
			if (buffer && instance) {
				strcat(buffer, instance);
			}
			len += instance_len;
			format += 9;
		} else if (strncmp(next, "%shell", 6) == 0) {
			if (buffer) {
				strcat(buffer, shell);
			}
			len += shell_len;
			format += 6;
		} else {
			if (buffer) {
				strcat(buffer, "%");
			}
			++format;
			++len;
		}
		next = strchr(format, '%');
	}
	if (buffer) {
		strcat(buffer, format);
	}
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
	container_update_title_textures(view->swayc);
	container_notify_child_title_changed(view->swayc->parent);
	config_update_font_height(false);
}
