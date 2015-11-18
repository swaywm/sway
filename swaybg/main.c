#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <time.h>
#include "client/client.h"
#include "log.h"

struct client_state *state;

void sway_terminate(void) {
	client_teardown(state);
	exit(1);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	if (!(state = client_setup(100, 100))) {
		return -1;
	}

	uint8_t r = 100, g = 100, b = 100;

	do {
		if (client_prerender(state)) {
			cairo_set_source_rgb(state->cairo, r / 256.0, g / 256.0, b / 256.0);
			cairo_rectangle(state->cairo, 0, 0, state->width, state->height);
			cairo_fill(state->cairo);

			client_render(state);

			r++; if (r == 0) { g++; if (g == 0) { b++; } }
		}
	} while (wl_display_dispatch(state->display) != -1);

	client_teardown(state);
	return 0;
}
