#ifndef _SWAYBAR_TRAY_H
#define _SWAYBAR_TRAY_H

#include <stdint.h>
#include <stdbool.h>
#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni.h"
#include "swaybar/bar.h"
#include "list.h"

struct tray {
	struct swaybar *bar;
	struct wl_list items;
};

/**
 * Processes a mouse event on the bar
 * TODO TRAY mouse
void tray_mouse_event(struct output *output, int x, int y,
		uint32_t button, uint32_t state);
*/

uint32_t render_tray(cairo_t *cairo, struct swaybar_output *output,
		struct swaybar_config *config, struct swaybar_workspace *ws,
		double *pos, uint32_t height);

/**
 * Initializes the tray with D-Bus
 */
void init_tray(struct swaybar *bar);

#endif /* _SWAYBAR_TRAY_H */
