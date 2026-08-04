#ifndef _SWAYBAR_TRAY_HOST_H
#define _SWAYBAR_TRAY_HOST_H

#include <stdbool.h>

#include "config.h"
#if HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif

struct swaybar_tray;

struct swaybar_host {
	struct swaybar_tray *tray;
	char *service;
	char *watcher_interface;
	sd_bus_slot *reg_slot;
	sd_bus_slot *unreg_slot;
	sd_bus_slot *watcher_slot;
};

bool init_host(struct swaybar_host *host, char *protocol, struct swaybar_tray *tray);
void finish_host(struct swaybar_host *host);

#endif
