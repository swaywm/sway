#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H

#include "container.h"
#include "focus.h"

// Position is absolute coordinates on the edge where the adjacent output
// should be searched for.
swayc_t *output_by_name(const char* name, const struct wlc_point *abs_pos);
swayc_t *swayc_adjacent_output(swayc_t *output, enum movement_direction dir, const struct wlc_point *abs_pos, bool pick_closest);

// Place absolute coordinates for given container into given wlc_point.
void get_absolute_position(swayc_t *container, struct wlc_point *point);

// Place absolute coordinates for the center point of given container into
// given wlc_point.
void get_absolute_center_position(swayc_t *container, struct wlc_point *point);

int sort_workspace_cmp_qsort(const void *a, const void *b);
void sort_workspaces(swayc_t *output);

#endif
