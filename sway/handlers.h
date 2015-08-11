#ifndef _SWAY_HANDLERS_H
#define _SWAY_HANDLERS_H

#include <stdbool.h>
#include <wlc/wlc.h>

bool handle_output_created(wlc_handle output);
void handle_output_destroyed(wlc_handle output);
void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to);
void handle_output_focused(wlc_handle output, bool focus);

bool handle_view_created(wlc_handle view);
void handle_view_destroyed(wlc_handle view);
void handle_view_focus(wlc_handle view, bool focus);
void handle_view_geometry_request(wlc_handle view, const struct wlc_geometry* geometry);

bool handle_key(wlc_handle view, uint32_t time, const struct wlc_modifiers
		*modifiers, uint32_t key, uint32_t sym, enum wlc_key_state state);

bool handle_pointer_motion(wlc_handle view, uint32_t time, const struct wlc_origin *origin);
bool handle_pointer_button(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t button, enum wlc_button_state state);

#endif
