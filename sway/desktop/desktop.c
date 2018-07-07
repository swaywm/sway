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
