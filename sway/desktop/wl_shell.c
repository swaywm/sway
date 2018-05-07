#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_wl_shell.h>
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/input/seat.h"
#include "sway/input/input-manager.h"
#include "log.h"

static struct sway_wl_shell_view *wl_shell_view_from_view(
		struct sway_view *view) {
	if (!sway_assert(view->type == SWAY_VIEW_WL_SHELL,
			"Expected wl_shell view")) {
		return NULL;
	}
	return (struct sway_wl_shell_view *)view;
}

static const char *get_string_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (wl_shell_view_from_view(view) == NULL) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_wl_shell_surface->title;
	case VIEW_PROP_CLASS:
		return view->wlr_wl_shell_surface->class;
	default:
		return NULL;
	}
}

static void configure(struct sway_view *view, double ox, double oy, int width,
		int height) {
	struct sway_wl_shell_view *wl_shell_view = wl_shell_view_from_view(view);
	if (wl_shell_view == NULL) {
		return;
	}
	wl_shell_view->pending_width = width;
	wl_shell_view->pending_height = height;
	wlr_wl_shell_surface_configure(view->wlr_wl_shell_surface, 0, width, height);
}

static void _close(struct sway_view *view) {
	if (wl_shell_view_from_view(view) == NULL) {
		return;
	}

	wl_client_destroy(view->wlr_wl_shell_surface->client);
}

static void destroy(struct sway_view *view) {
	struct sway_wl_shell_view *wl_shell_view = wl_shell_view_from_view(view);
	if (wl_shell_view == NULL) {
		return;
	}
	wl_list_remove(&wl_shell_view->commit.link);
	wl_list_remove(&wl_shell_view->destroy.link);
	wl_list_remove(&wl_shell_view->request_fullscreen.link);
	wl_list_remove(&wl_shell_view->set_state.link);
	free(wl_shell_view);
}

static void set_fullscreen(struct sway_view *view, bool fullscreen) {
	// TODO
}

static const struct sway_view_impl view_impl = {
	.get_string_prop = get_string_prop,
	.configure = configure,
	.close = _close,
	.destroy = destroy,
	.set_fullscreen = set_fullscreen,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_view *wl_shell_view =
		wl_container_of(listener, wl_shell_view, commit);
	struct sway_view *view = &wl_shell_view->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Let floating views do whatever
	view_update_size(view, wl_shell_view->pending_width,
		wl_shell_view->pending_height);
	view_damage(view, false);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_view *wl_shell_view =
		wl_container_of(listener, wl_shell_view, destroy);
	view_destroy(&wl_shell_view->view);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_view *wl_shell_view =
		wl_container_of(listener, wl_shell_view, request_fullscreen);
	view_set_fullscreen(&wl_shell_view->view, true);
}

static void handle_set_state(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_view *wl_shell_view =
		wl_container_of(listener, wl_shell_view, set_state);
	struct sway_view *view = &wl_shell_view->view;
	struct wlr_wl_shell_surface *surface = view->wlr_wl_shell_surface;
	if (view->is_fullscreen &&
			surface->state != WLR_WL_SHELL_SURFACE_STATE_FULLSCREEN) {
		view_set_fullscreen(view, false);
	}
}

void handle_wl_shell_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server,
		wl_shell_surface);
	struct wlr_wl_shell_surface *shell_surface = data;

	if (shell_surface->state == WLR_WL_SHELL_SURFACE_STATE_POPUP) {
		// popups don't get views
		wlr_log(L_DEBUG, "New wl_shell popup");
		return;
	}

	// TODO: make transient windows floating

	wlr_log(L_DEBUG, "New wl_shell toplevel title='%s' app_id='%s'",
			shell_surface->title, shell_surface->class);
	wlr_wl_shell_surface_ping(shell_surface);

	struct sway_wl_shell_view *wl_shell_view =
		calloc(1, sizeof(struct sway_wl_shell_view));
	if (!sway_assert(wl_shell_view, "Failed to allocate view")) {
		return;
	}

	view_init(&wl_shell_view->view, SWAY_VIEW_WL_SHELL, &view_impl);
	wl_shell_view->view.wlr_wl_shell_surface = shell_surface;

	// TODO:
	// - Wire up listeners
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria

	wl_shell_view->commit.notify = handle_commit;
	wl_signal_add(&shell_surface->surface->events.commit,
		&wl_shell_view->commit);

	wl_shell_view->destroy.notify = handle_destroy;
	wl_signal_add(&shell_surface->events.destroy, &wl_shell_view->destroy);

	wl_shell_view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&shell_surface->events.request_fullscreen,
			&wl_shell_view->request_fullscreen);

	wl_shell_view->set_state.notify = handle_set_state;
	wl_signal_add(&shell_surface->events.set_state, &wl_shell_view->set_state);

	view_map(&wl_shell_view->view, shell_surface->surface);

	if (shell_surface->state == WLR_WL_SHELL_SURFACE_STATE_FULLSCREEN) {
		view_set_fullscreen(&wl_shell_view->view, true);
	}
}
