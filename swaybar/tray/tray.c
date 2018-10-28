#include <cairo.h>
#include <stdint.h>
#include <stdlib.h>
#include "swaybar/bar.h"
#include "swaybar/tray/tray.h"
#include "log.h"

struct swaybar_tray *create_tray(struct swaybar *bar) {
	wlr_log(WLR_DEBUG, "Initializing tray");
	return NULL;
}

void destroy_tray(struct swaybar_tray *tray) {
}

void tray_in(int fd, short mask, void *data) {
}

uint32_t render_tray(cairo_t *cairo, struct swaybar_output *output, double *x) {
	return 0; // placeholder
}
