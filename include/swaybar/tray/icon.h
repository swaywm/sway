#ifndef _SWAYBAR_ICON_H
#define _SWAYBAR_ICON_H

#include <stdint.h>
#include <stdbool.h>
#include "cairo.h"

/**
 * Returns the image found by `name` that is closest to `size`
 */
cairo_surface_t *find_icon(const char *name, int size);

/* Struct used internally only */
struct subdir;

#endif /* _SWAYBAR_ICON_H */
