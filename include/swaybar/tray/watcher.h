#ifndef _SWAYBAR_TRAY_WATCHER_H
#define _SWAYBAR_TRAY_WATCHER_H

#include "swaybar/tray/tray.h"
#include "list.h"

struct swaybar_watcher {
	char *interface;
	sd_bus *bus;
	list_t *hosts;
	list_t *items;
	int version;
};

struct swaybar_watcher *create_watcher(char *protocol, sd_bus *bus);
void destroy_watcher(struct swaybar_watcher *watcher);

#endif
