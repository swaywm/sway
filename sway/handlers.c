#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "log.h"
#include "handlers.h"

bool handle_output_created(wlc_handle output) {
	add_output(output);
	return true;
}

void handle_output_destroyed(wlc_handle output) {
	destroy_output(output);
}

void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
	sway_log(L_DEBUG, "Output %d resolution changed to %d x %d", output, to->w, to->h);
	swayc_t *c = get_swayc_for_handle(output, &root_container);
	if (!c) return;
	c->width = to->w;
	c->height = to->h;
	arrange_windows(&root_container, -1, -1);
}

bool handle_view_created(wlc_handle view) {
	add_view(view);
	return true;
}

void handle_view_destroyed(wlc_handle view) {
	destroy_view(get_swayc_for_handle(view, &root_container));
	return true;
}

void handle_view_focus(wlc_handle view, bool focus) {
	wlc_view_set_state(view, WLC_BIT_ACTIVATED, focus);
	focus_view(get_swayc_for_handle(view, &root_container));
}

void handle_view_geometry_request(wlc_handle view, const struct wlc_geometry* geometry) {
	// deny that shit
}
