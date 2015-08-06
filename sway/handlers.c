#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "handlers.h"

bool handle_output_created(wlc_handle output) {
	return true;
}

void handle_output_destroyed(wlc_handle output) {
}

void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
}

bool handle_view_created(wlc_handle view) {
	printf("View created, focusing");
	wlc_view_focus(view);
	wlc_view_bring_to_front(view);
	return true;
}

void handle_view_destroyed(wlc_handle view) {
	printf("View destroyed");
	wlc_view_focus(get_topmost(wlc_view_get_output(view), 0));
	return true;
}

void handle_view_focus(wlc_handle view, bool focus) {
	wlc_view_set_state(view, WLC_BIT_ACTIVATED, focus);
}
