#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include "client.h"
#include "log.h"

struct client_state *state;

void sway_terminate(void) {
	client_teardown(state);
	exit(1);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	state = client_setup();

	uint8_t r = 0, g = 0, b = 0;

	int rs;
	do {
		if (!client_prerender(state)) continue;
		cairo_set_source_rgb(state->cairo, r, g, b);
		cairo_rectangle(state->cairo, 0, 0, 100, 100);
		cairo_fill(state->cairo);

		rs = client_render(state);

		if (rs == 1) {
			sway_log(L_INFO, "rendering %d %d %d", r, g, b);
			r++; g++; b++;
		}
	} while (rs);

	client_teardown(state);
	return 0;
}
