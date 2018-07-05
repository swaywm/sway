#ifndef _SWAY_RENDER_H
#define _SWAY_RENDER_H
#include <pixman.h>
#include "sway/output.h"

void render_output(struct sway_output *output, struct timespec *when,
		pixman_region32_t *damage);

#endif
