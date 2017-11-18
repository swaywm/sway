#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output.h>
#include "container.h"
#include "focus.h"

struct sway_server;

struct sway_output {
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct sway_server *server;
	struct timespec last_frame;
};

// Position is absolute coordinates on the edge where the adjacent output
// should be searched for.
swayc_t *output_by_name(const char* name, const struct wlc_point *abs_pos);
swayc_t *swayc_opposite_output(enum movement_direction dir, const struct wlc_point *abs_pos);
swayc_t *swayc_adjacent_output(swayc_t *output, enum movement_direction dir, const struct wlc_point *abs_pos, bool pick_closest);

// Place absolute coordinates for given container into given wlc_point.
void get_absolute_position(swayc_t *container, struct wlc_point *point);

// Place absolute coordinates for the center point of given container into
// given wlc_point.
void get_absolute_center_position(swayc_t *container, struct wlc_point *point);

// stable sort workspaces on this output
void sort_workspaces(swayc_t *output);

void output_get_scaled_size(wlc_handle handle, struct wlc_size *size);

#endif
