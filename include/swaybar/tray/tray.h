#ifndef _SWAYBAR_TRAY_H
#define _SWAYBAR_TRAY_H

#include <stdint.h>
#include <stdbool.h>
#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni.h"
#include "swaybar/bar.h"
#include "list.h"

extern struct tray *tray;

struct tray {
	list_t *items;
};

/**
 * Processes a mouse event on the bar
 */
void tray_mouse_event(struct output *output, int x, int y,
		uint32_t button, uint32_t state);

uint32_t tray_render(struct output *output, struct config *config);

void tray_upkeep(struct bar *bar);

/**
 * Initializes the tray with D-Bus
 */
void init_tray();

#endif /* _SWAYBAR_TRAY_H */
