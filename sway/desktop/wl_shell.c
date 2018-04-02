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

static bool assert_wl_shell(struct sway_view *view) {
	return sway_assert(view->type == SWAY_WL_SHELL_VIEW,
		"Expecting wl_shell view!");
}

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (!assert_wl_shell(view)) {
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
	if (!assert_wl_shell(view)) {
		return;
	}
	view_update_position(view, ox, oy);
	view->sway_wl_shell_surface->pending_width = width;
	view->sway_wl_shell_surface->pending_height = height;
	wlr_wl_shell_surface_configure(view->wlr_wl_shell_surface, 0, width, height);
}

static void _close(struct sway_view *view) {
	if (!assert_wl_shell(view)) {
		return;
	}

	wl_client_destroy(view->wlr_wl_shell_surface->client);
}

static const struct sway_view_impl view_impl = {
	.get_prop = get_prop,
	.configure = configure,
	.close = _close,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_surface *sway_surface =
		wl_container_of(listener, sway_surface, commit);
	struct sway_view *view = sway_surface->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Let floating views do whatever
	view_update_size(view, sway_surface->pending_width,
		sway_surface->pending_height);
	view_damage_from(view);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_surface *sway_surface =
		wl_container_of(listener, sway_surface, destroy);
	wl_list_remove(&sway_surface->commit.link);
	wl_list_remove(&sway_surface->destroy.link);
	view_destroy(sway_surface->view);
	free(sway_surface);
}

void handle_wl_shell_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server,
		wl_shell_surface);
	struct wlr_wl_shell_surface *shell_surface = data;

	if (shell_surface->state == WLR_WL_SHELL_SURFACE_STATE_POPUP) {
		// popups don't get views
		return;
	}

	// TODO make transient windows floating

	wlr_log(L_DEBUG, "New wl_shell toplevel title='%s' app_id='%s'",
			shell_surface->title, shell_surface->class);
	wlr_wl_shell_surface_ping(shell_surface);

	struct sway_wl_shell_surface *sway_surface =
		calloc(1, sizeof(struct sway_wl_shell_surface));
	if (!sway_assert(sway_surface, "Failed to allocate surface!")) {
		return;
	}

	struct sway_view *view = view_create(SWAY_WL_SHELL_VIEW, &view_impl);
	if (!sway_assert(view, "Failed to allocate view")) {
		return;
	}
	view->wlr_wl_shell_surface = shell_surface;
	view->sway_wl_shell_surface = sway_surface;
	sway_surface->view = view;

	// TODO:
	// - Wire up listeners
	// - Handle popups
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria

	sway_surface->commit.notify = handle_commit;
	wl_signal_add(&shell_surface->surface->events.commit,
		&sway_surface->commit);

	sway_surface->destroy.notify = handle_destroy;
	wl_signal_add(&shell_surface->events.destroy, &sway_surface->destroy);

	view_map(view, shell_surface->surface);
}
