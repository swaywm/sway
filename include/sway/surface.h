#ifndef _SWAY_SURFACE_H
#define _SWAY_SURFACE_H
#include <wlr/types/wlr_compositor.h>

struct sway_surface {
	struct wlr_surface *wlr_surface;

	struct wl_listener destroy;

	/**
	 * This timer can be used for issuing delayed frame done callbacks (for
	 * example, to improve presentation latency). Its handler is set to a
	 * function that issues a frame done callback to this surface.
	 */
	struct wl_event_source *frame_done_timer;
};

void surface_update_outputs(struct wlr_surface *surface);
void surface_enter_output(struct wlr_surface *surface,
	struct sway_output *output);
void surface_leave_output(struct wlr_surface *surface,
	struct sway_output *output);

#endif
