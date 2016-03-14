#ifndef _SWAY_RENDER_H
#define _SWAY_RENDER_H
#include <wlc/wlc.h>
#include "container.h"

void render_view_borders(wlc_handle view);
void update_view_border(swayc_t *view);

#endif
