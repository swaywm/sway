#include "wayland-desktop-shell-client-protocol.h"
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
	if (!(state = client_setup(100, 100, false))) {
		return -1;
	}
	if (!state->desktop_shell) {
		sway_abort("swaybg requires the compositor to support the desktop-shell extension.");
	}
	struct output_state *output = state->outputs->items[0];
	state->width = output->width;
	state->height = output->height;
	desktop_shell_set_background(state->desktop_shell, output->output, state->surface);

	uint8_t r = 0, g = 0, b = 0;

	do {
		if (client_prerender(state) && state->cairo) {
			cairo_set_source_rgb(state->cairo, r / 256.0, g / 256.0, b / 256.0);
			cairo_rectangle(state->cairo, 0, 0, state->width, state->height);
			cairo_fill(state->cairo);

			client_render(state);

			r++; g += 2; b += 4;
		}
	} while (wl_display_dispatch(state->display) != -1);

	client_teardown(state);
	return 0;
}
