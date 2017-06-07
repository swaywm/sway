#ifndef _SWAYBAR_TRAY_H
#define _SWAYBAR_TRAY_H

#include <stdint.h>
#include <stdbool.h>
#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni.h"
#include "list.h"

extern struct tray *tray;

struct tray {
	list_t *items;
};

/**
 * Initializes the tray host with D-Bus
 */
int init_tray();

/**
 * Returns an item if `x` and `y` collide with it and NULL otherwise
 */
struct StatusNotifierItem *collides_with_sni(int x, int y);

#endif /* _SWAYBAR_TRAY_H */
