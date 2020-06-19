#include "sway/tree/container.h"
#include "sway/desktop.h"
#include "sway/output.h"

void desktop_damage_surface(struct wlr_surface *surface, double lx, double ly,
		bool whole) {
	struct sway_output *output;
	list_for_each(output, root->outputs) {
		struct wlr_box *output_box = wlr_output_layout_get_box(
			root->output_layout, output->wlr_output);
		output_damage_surface(output, lx - output_box->x,
			ly - output_box->y, surface, whole);
	}
}

void desktop_damage_whole_container(struct sway_container *con) {
	struct sway_output *output;
	list_for_each(output, root->outputs) {
		output_damage_whole_container(output, con);
	}
}

void desktop_damage_box(struct wlr_box *box) {
	struct sway_output *output;
	list_for_each(output, root->outputs) {
		output_damage_box(output, box);
	}
}

void desktop_damage_view(struct sway_view *view) {
	desktop_damage_whole_container(view->container);
	struct wlr_box box = {
		.x = view->container->current.content_x - view->geometry.x,
		.y = view->container->current.content_y - view->geometry.y,
		.width = view->surface->current.width,
		.height = view->surface->current.height,
	};
	desktop_damage_box(&box);
}
