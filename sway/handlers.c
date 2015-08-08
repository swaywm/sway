#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "handlers.h"

bool handle_output_created(wlc_handle output) {
	add_output(output);
	return true;
}

void handle_output_destroyed(wlc_handle output) {
	destroy_output(output);
}

void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
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
