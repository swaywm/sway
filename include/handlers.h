#ifndef _SWAY_HANDLERS_H
#define _SWAY_HANDLERS_H

#include <stdbool.h>
#include <wlc/wlc.h>

extern struct wlc_interface interface;

//set focus to current pointer location and return focused container
swayc_t *focus_pointer(void);

#endif
