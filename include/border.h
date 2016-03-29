#ifndef _SWAY_BORDER_H
#define _SWAY_BORDER_H
#include <wlc/wlc.h>
#include "container.h"

void render_view_borders(wlc_handle view);
void update_view_border(swayc_t *view);
int get_font_text_height(const char *font);

#endif
