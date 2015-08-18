#ifndef _SWAY_HANDLERS_H
#define _SWAY_HANDLERS_H
#include "container.h"
#include <stdbool.h>
#include <wlc/wlc.h>

extern struct wlc_interface interface;

swayc_t *container_under_pointer(void);

#endif
