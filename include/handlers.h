#ifndef _SWAY_HANDLERS_H
#define _SWAY_HANDLERS_H
#include "container.h"
#include <stdbool.h>
#include <wlc/wlc.h>

void register_wlc_handlers();

extern uint32_t keys_pressed[32];

#endif
