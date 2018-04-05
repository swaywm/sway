#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include "log.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"

void view_init(struct sway_view *view, enum sway_view_type type,
		const struct sway_view_impl *impl) {
	view->type = type;
	view->impl = impl;
	wl_signal_init(&view->events.unmap);
}

void view_destroy(struct sway_view *view) {
	if (view == NULL) {
		return;
	}

	if (view->surface != NULL) {
		view_unmap(view);
	}

	container_destroy(view->swayc);

	if (view->impl->destroy) {
		view->impl->destroy(view);
	} else {
		free(view);
	}
}

const char *view_get_title(struct sway_view *view) {
	if (view->impl->get_prop) {
		return view->impl->get_prop(view, VIEW_PROP_TITLE);
	}
	return NULL;
}

const char *view_get_app_id(struct sway_view *view) {
	if (view->impl->get_prop) {
		return view->impl->get_prop(view, VIEW_PROP_APP_ID);
	}
	return NULL;
}

const char *view_get_class(struct sway_view *view) {
	if (view->impl->get_prop) {
		return view->impl->get_prop(view, VIEW_PROP_CLASS);
	}
	return NULL;
}

const char *view_get_instance(struct sway_view *view) {
	if (view->impl->get_prop) {
		return view->impl->get_prop(view, VIEW_PROP_INSTANCE);
	}
	return NULL;
}

void view_configure(struct sway_view *view, double ox, double oy, int width,
		int height) {
	if (view->impl->configure) {
		view->impl->configure(view, ox, oy, width, height);
	}
}

void view_set_activated(struct sway_view *view, bool activated) {
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
}

void view_close(struct sway_view *view) {
	if (view->impl->close) {
		view->impl->close(view);
	}
}

void view_damage_whole(struct sway_view *view) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_whole_view(cont->sway_output, view);
		}
	}
}

void view_damage_from(struct sway_view *view) {
	// TODO
	view_damage_whole(view);
}

static void view_get_layout_box(struct sway_view *view, struct wlr_box *box) {
	struct sway_container *output = container_parent(view->swayc, C_OUTPUT);

	box->x = output->x + view->swayc->x;
	box->y = output->y + view->swayc->y;
	box->width = view->width;
	box->height = view->height;
}

static void view_update_outputs(struct sway_view *view,
		const struct wlr_box *before) {
	struct wlr_box box;
	view_get_layout_box(view, &box);

	struct wlr_output_layout *output_layout =
		root_container.sway_root->output_layout;
	struct wlr_output_layout_output *layout_output;
	wl_list_for_each(layout_output, &output_layout->outputs, link) {
		bool intersected = before != NULL && wlr_output_layout_intersects(
			output_layout, layout_output->output, before);
		bool intersects = wlr_output_layout_intersects(output_layout,
			layout_output->output, &box);
		if (intersected && !intersects) {
			wlr_surface_send_leave(view->surface, layout_output->output);
		}
		if (!intersected && intersects) {
			wlr_surface_send_enter(view->surface, layout_output->output);
		}
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

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface) {
	if (!sway_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus_inactive(seat,
		&root_container);
	struct sway_container *cont = container_view_create(focus, view);

	view->surface = wlr_surface;
	view->swayc = cont;

	view_init_subsurfaces(view, wlr_surface);
	wl_signal_add(&wlr_surface->events.new_subsurface,
		&view->surface_new_subsurface);
	view->surface_new_subsurface.notify = view_handle_surface_new_subsurface;

	arrange_windows(cont->parent, -1, -1);
	input_manager_set_focus(input_manager, cont);

	view_damage_whole(view);
	view_update_outputs(view, NULL);
}

void view_unmap(struct sway_view *view) {
	if (!sway_assert(view->surface != NULL, "cannot unmap unmapped view")) {
		return;
	}

	wl_signal_emit(&view->events.unmap, view);

	view_damage_whole(view);

	struct sway_container *parent = container_destroy(view->swayc);

	wl_list_remove(&view->surface_new_subsurface.link);

	view->swayc = NULL;
	view->surface = NULL;

	arrange_windows(parent, -1, -1);
}

void view_update_position(struct sway_view *view, double ox, double oy) {
	if (view->swayc->x == ox && view->swayc->y == oy) {
		return;
	}

	struct wlr_box box;
	view_get_layout_box(view, &box);
	view_damage_whole(view);
	view->swayc->x = ox;
	view->swayc->y = oy;
	view_update_outputs(view, &box);
	view_damage_whole(view);
}

void view_update_size(struct sway_view *view, int width, int height) {
	if (view->width == width && view->height == height) {
		return;
	}

	struct wlr_box box;
	view_get_layout_box(view, &box);
	view_damage_whole(view);
	view->width = width;
	view->height = height;
	view_update_outputs(view, &box);
	view_damage_whole(view);
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
	wl_list_for_each(subsurface, &surface->subsurface_list, parent_link) {
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

	view_init_subsurfaces(child->view, surface);

	// TODO: only damage the whole child
	view_damage_whole(child->view);
}

void view_child_destroy(struct sway_view_child *child) {
	// TODO: only damage the whole child
	view_damage_whole(child->view);

	wl_list_remove(&child->surface_commit.link);
	wl_list_remove(&child->surface_destroy.link);
	wl_list_remove(&child->view_unmap.link);

	if (child->impl && child->impl->destroy) {
		child->impl->destroy(child);
	} else {
		free(child);
	}
}
