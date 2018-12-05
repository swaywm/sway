#ifndef _SWAYBAR_TRAY_TRAY_H
#define _SWAYBAR_TRAY_TRAY_H

#include "config.h"
#ifdef HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#endif
#include <cairo.h>
#include <stdint.h>

struct swaybar;
struct swaybar_output;
struct swaybar_watcher;

struct swaybar_tray {
	struct swaybar *bar;

	int fd;
	sd_bus *bus;

	struct swaybar_watcher *watcher_xdg;
	struct swaybar_watcher *watcher_kde;
};

struct swaybar_tray *create_tray(struct swaybar *bar);
void destroy_tray(struct swaybar_tray *tray);
void tray_in(int fd, short mask, void *data);
uint32_t render_tray(cairo_t *cairo, struct swaybar_output *output, double *x);

#endif
