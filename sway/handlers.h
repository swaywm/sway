#ifndef _SWAY_HANDLERS_H
#define _SWAY_HANDLERS_H

#include <stdbool.h>
#include <wlc/wlc.h>

bool handle_output_created(wlc_handle output);
void handle_output_destroyed(wlc_handle output);
void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to);

bool handle_view_created(wlc_handle view);
void handle_view_destroyed(wlc_handle view);
void handle_view_focus(wlc_handle view, bool focus);

#endif
