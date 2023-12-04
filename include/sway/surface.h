#ifndef _SWAY_SURFACE_H
#define _SWAY_SURFACE_H
#include <wlr/types/wlr_compositor.h>

void surface_enter_output(struct wlr_surface *surface,
	struct sway_output *output);
void surface_leave_output(struct wlr_surface *surface,
	struct sway_output *output);

#endif
