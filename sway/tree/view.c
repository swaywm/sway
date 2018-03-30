#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/view.h"

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

void view_update_outputs(struct sway_view *view, const struct wlr_box *before) {
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
