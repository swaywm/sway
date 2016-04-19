#ifndef _SWAY_BORDER_H
#define _SWAY_BORDER_H
#include <wlc/wlc.h>
#include "container.h"

struct border {
	unsigned char *buffer;
	struct wlc_geometry geometry;
};

void render_view_borders(wlc_handle view);
void update_view_border(swayc_t *view);
void map_update_view_border(swayc_t *view, void *data);
int get_font_text_height(const char *font);

#endif
