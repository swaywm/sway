#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <time.h>
#include "client.h"
#include "log.h"

struct client_state *state;

void sway_terminate(void) {
	client_teardown(state);
	exit(1);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	if (!(state = client_setup())) {
		return -1;
	}

	uint8_t r = 0, g = 0, b = 0;

	long last_ms = 0;
	int rs;
	do {
		struct timespec spec;
		clock_gettime(CLOCK_MONOTONIC, &spec);
		long ms = round(spec.tv_nsec / 1.0e6);

		cairo_set_source_rgb(state->cairo, r, g, b);
		cairo_rectangle(state->cairo, 0, 0, 100, 100);
		cairo_fill(state->cairo);

		rs = client_render(state);

		if (ms - last_ms > 100) {
			r++;
			if (r == 0) {
				g++;
				if (g == 0) {
					b++;
				}
			}
			ms = last_ms;
		}
	} while (rs);

	client_teardown(state);
	return 0;
}
