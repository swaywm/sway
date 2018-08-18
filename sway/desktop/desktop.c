#include "sway/tree/container.h"
#include "sway/desktop.h"
#include "sway/output.h"

void desktop_damage_surface(struct wlr_surface *surface, double lx, double ly,
		bool whole) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_surface(cont->sway_output,
				lx - cont->current.swayc_x, ly - cont->current.swayc_y,
				surface, whole);
		}
	}
}

void desktop_damage_whole_container(struct sway_container *con) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_whole_container(cont->sway_output, con);
		}
	}
}

void desktop_damage_box(struct wlr_box *box) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		output_damage_box(cont->sway_output, box);
	}
}

void desktop_damage_view(struct sway_view *view) {
	desktop_damage_whole_container(view->swayc);
	struct wlr_box box = {
		.x = view->swayc->current.view_x - view->geometry.x,
		.y = view->swayc->current.view_y - view->geometry.y,
		.width = view->surface->current.width,
		.height = view->surface->current.height,
	};
	desktop_damage_box(&box);
}
