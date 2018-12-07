#ifndef _SWAYBAR_TRAY_HOST_H
#define _SWAYBAR_TRAY_HOST_H

#include <stdbool.h>

struct swaybar_tray;

struct swaybar_host {
	struct swaybar_tray *tray;
	char *service;
	char *watcher_interface;
};

bool init_host(struct swaybar_host *host, char *protocol, struct swaybar_tray *tray);
void finish_host(struct swaybar_host *host);

#endif
