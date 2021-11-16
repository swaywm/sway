#ifndef _SWAYBAR_TRAY_TRAY_H
#define _SWAYBAR_TRAY_TRAY_H

#include "config.h"
#if HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif
#include <cairo.h>
#include <stdint.h>
#include "swaybar/tray/host.h"
#include "list.h"

struct swaybar;
struct swaybar_output;
struct swaybar_watcher;

struct swaybar_tray {
	struct swaybar *bar;

	int fd;
	sd_bus *bus;

	struct swaybar_host host_xdg;
	struct swaybar_host host_kde;
	list_t *items; // struct swaybar_sni *
	struct swaybar_watcher *watcher_xdg;
	struct swaybar_watcher *watcher_kde;

	list_t *basedirs; // char *
	list_t *themes; // struct swaybar_theme *
};

struct swaybar_tray *create_tray(struct swaybar *bar);
void destroy_tray(struct swaybar_tray *tray);
void tray_in(int fd, short mask, void *data);
uint32_t render_tray(cairo_t *cairo, struct swaybar_output *output, double *x);

#endif
