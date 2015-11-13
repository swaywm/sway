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

	do {
		if (!client_prerender(state)) continue;
		cairo_set_source_rgb(state->cairo, 255, 0, 0);
		cairo_rectangle(state->cairo, 0, 0, 100, 100);
		cairo_fill(state->cairo);
	} while (client_render(state));

	client_teardown(state);
	return 0;
}
