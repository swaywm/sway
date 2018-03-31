#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include "log.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"

struct sway_view *view_create(enum sway_view_type type) {
	struct sway_view *view = calloc(1, sizeof(struct sway_view));
	if (view == NULL) {
		return NULL;
	}
	view->type = type;
	wl_list_init(&view->unmanaged_view_link);
	return view;
}

void view_destroy(struct sway_view *view) {
	if (view == NULL) {
		return;
	}

	if (view->surface != NULL) {
		view_unmap(view);
	}
	if (view->swayc != NULL) {
		container_view_destroy(view->swayc);
	}

	free(view);
}

const char *view_get_title(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_TITLE);
	}
	return NULL;
}

const char *view_get_app_id(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_APP_ID);
	}
	return NULL;
}

const char *view_get_class(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_CLASS);
	}
	return NULL;
}

const char *view_get_instance(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_INSTANCE);
	}
	return NULL;
}

static void view_update_outputs(struct sway_view *view,
		const struct wlr_box *before) {
	struct wlr_output_layout *output_layout =
		root_container.sway_root->output_layout;
	struct wlr_box box = {
		.x = view->swayc->x,
		.y = view->swayc->y,
		.width = view->width,
		.height = view->height,
	};
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

void view_set_size(struct sway_view *view, int width, int height) {
	if (view->iface.set_size) {
		struct wlr_box box = {
			.x = view->swayc->x,
			.y = view->swayc->y,
			.width = view->width,
			.height = view->height,
		};
		view->iface.set_size(view, width, height);
		view_update_outputs(view, &box);
	}
}

// TODO make view coordinates in layout coordinates
void view_set_position(struct sway_view *view, double ox, double oy) {
	if (view->iface.set_position) {
		struct wlr_box box = {
			.x = view->swayc->x,
			.y = view->swayc->y,
			.width = view->width,
			.height = view->height,
		};
		view->iface.set_position(view, ox, oy);
		view_update_outputs(view, &box);
	}
}

void view_set_activated(struct sway_view *view, bool activated) {
	if (view->iface.set_activated) {
		view->iface.set_activated(view, activated);
	}
}

void view_close(struct sway_view *view) {
	if (view->iface.close) {
		view->iface.close(view);
	}
}

struct sway_container *container_view_destroy(struct sway_container *view) {
	if (!view) {
		return NULL;
	}
	wlr_log(L_DEBUG, "Destroying view '%s'", view->name);
	struct sway_container *parent = container_destroy(view);
	arrange_windows(parent, -1, -1);
	return parent;
}

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface) {
	if (!sway_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = sway_seat_get_focus_inactive(seat,
		&root_container);
	struct sway_container *cont = container_view_create(focus, view);

	view->surface = wlr_surface;
	view->swayc = cont;

	arrange_windows(cont->parent, -1, -1);
	sway_input_manager_set_focus(input_manager, cont);

	view_damage_whole(view);
}

void view_map_unmanaged(struct sway_view *view,
		struct wlr_surface *wlr_surface) {
	if (!sway_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}

	view->surface = wlr_surface;
	view->swayc = NULL;

	wl_list_insert(&root_container.sway_root->unmanaged_views,
		&view->unmanaged_view_link);

	view_damage_whole(view);
}

void view_unmap(struct sway_view *view) {
	if (!sway_assert(view->surface != NULL, "cannot unmap unmapped view")) {
		return;
	}

	view_damage_whole(view);

	wl_list_remove(&view->unmanaged_view_link);
	wl_list_init(&view->unmanaged_view_link);

	container_view_destroy(view->swayc);

	view->swayc = NULL;
	view->surface = NULL;
}

void view_damage_whole(struct sway_view *view) {
	struct sway_container *cont = NULL;
	for (int i = 0; i < root_container.children->length; ++i) {
		cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_whole_view(cont->sway_output, view);
		}
	}
}

void view_damage_from(struct sway_view *view) {
	// TODO
	view_damage_whole(view);
}
