#include <cairo.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "swaybar/bar.h"
#include "swaybar/tray/tray.h"
#include "swaybar/tray/watcher.h"
#include "log.h"

struct swaybar_tray *create_tray(struct swaybar *bar) {
	wlr_log(WLR_DEBUG, "Initializing tray");

	sd_bus *bus;
	int ret = sd_bus_open_user(&bus);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to connect to user bus: %s", strerror(-ret));
		return NULL;
	}

	struct swaybar_tray *tray = calloc(1, sizeof(struct swaybar_tray));
	if (!tray) {
		return NULL;
	}
	tray->bar = bar;
	tray->bus = bus;
	tray->fd = sd_bus_get_fd(tray->bus);

	tray->watcher_xdg = create_watcher("freedesktop", tray->bus);
	tray->watcher_kde = create_watcher("kde", tray->bus);

	return tray;
}

void destroy_tray(struct swaybar_tray *tray) {
	if (!tray) {
		return;
	}
	destroy_watcher(tray->watcher_xdg);
	destroy_watcher(tray->watcher_kde);
	sd_bus_flush_close_unref(tray->bus);
	free(tray);
}

void tray_in(int fd, short mask, void *data) {
	sd_bus *bus = data;
	int ret;
	while ((ret = sd_bus_process(bus, NULL)) > 0) {
		// This space intentionally left blank
	}
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to process bus: %s", strerror(-ret));
	}
}

uint32_t render_tray(cairo_t *cairo, struct swaybar_output *output, double *x) {
	return 0; // placeholder
}
