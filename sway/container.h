#ifndef _SWAY_CONTAINER_H
#define _SWAY_CONTAINER_H

#include "layout.h"

void container_map(swayc_t *, void (*f)(swayc_t *, void *), void *);

#endif
