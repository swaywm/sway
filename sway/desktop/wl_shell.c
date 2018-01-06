#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_wl_shell.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/server.h"
#include "sway/view.h"
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

static void set_size(struct sway_view *view, int width, int height) {
	if (!assert_wl_shell(view)) {
		return;
	}
	view->sway_wl_shell_surface->pending_width = width;
	view->sway_wl_shell_surface->pending_height = height;
	wlr_wl_shell_surface_configure(view->wlr_wl_shell_surface, 0, width, height);
}

static void set_position(struct sway_view *view, double ox, double oy) {
	if (!assert_wl_shell(view)) {
		return;
	}
	view->swayc->x = ox;
	view->swayc->y = oy;
}

static void set_activated(struct sway_view *view, bool activated) {
	// no way to activate wl_shell
}

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_surface *sway_surface =
		wl_container_of(listener, sway_surface, commit);
	struct sway_view *view = sway_surface->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Let floating views do whatever
	view->width = sway_surface->pending_width;
	view->height = sway_surface->pending_height;
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_wl_shell_surface *sway_surface =
		wl_container_of(listener, sway_surface, destroy);
	wl_list_remove(&sway_surface->commit.link);
	wl_list_remove(&sway_surface->destroy.link);
	swayc_t *parent = destroy_view(sway_surface->view->swayc);
	free(sway_surface->view);
	free(sway_surface);
	arrange_windows(parent, -1, -1);
}

void handle_wl_shell_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(
			listener, server, wl_shell_surface);
	struct wlr_wl_shell_surface *shell_surface = data;

	if (shell_surface->state != WLR_WL_SHELL_SURFACE_STATE_TOPLEVEL) {
		// TODO: transient and popups should be floating
		return;
	}

	wlr_log(L_DEBUG, "New wl_shell toplevel title='%s' app_id='%s'",
			shell_surface->title, shell_surface->class);
	wlr_wl_shell_surface_ping(shell_surface);

	struct sway_wl_shell_surface *sway_surface =
		calloc(1, sizeof(struct sway_wl_shell_surface));
	if (!sway_assert(sway_surface, "Failed to allocate surface!")) {
		return;
	}

	struct sway_view *sway_view = calloc(1, sizeof(struct sway_view));
	if (!sway_assert(sway_view, "Failed to allocate view!")) {
		return;
	}
	sway_view->type = SWAY_WL_SHELL_VIEW;
	sway_view->iface.get_prop = get_prop;
	sway_view->iface.set_size = set_size;
	sway_view->iface.set_position = set_position;
	sway_view->iface.set_activated = set_activated;
	sway_view->wlr_wl_shell_surface = shell_surface;
	sway_view->sway_wl_shell_surface = sway_surface;
	sway_view->surface = shell_surface->surface;
	sway_surface->view = sway_view;

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

	// TODO: actual focus semantics
	swayc_t *parent = root_container.children->items[0];
	parent = parent->children->items[0]; // workspace

	swayc_t *cont = new_view(parent, sway_view);
	sway_view->swayc = cont;

	arrange_windows(cont->parent, -1, -1);

	sway_input_manager_set_focus(input_manager, cont);
}
