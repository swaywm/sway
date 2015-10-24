#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H

#include "container.h"
#include "focus.h"

swayc_t *output_by_name(const char* name);
swayc_t *swayc_adjacent_output(swayc_t *output, enum movement_direction dir);

#endif
