#include "wayland-swaylock-client-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include "client/window.h"
#include "client/registry.h"
#include "log.h"

list_t *surfaces;
struct registry *registry;

enum scaling_mode {
	SCALING_MODE_STRETCH,
	SCALING_MODE_FILL,
	SCALING_MODE_FIT,
	SCALING_MODE_CENTER,
	SCALING_MODE_TILE,
};

void sway_terminate(void) {
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	surfaces = create_list();
	registry = registry_poll();

	if (!registry->swaylock) {
		sway_abort("swaylock requires the compositor to support the swaylock extension.");
	}

	int i;
	for (i = 0; i < registry->outputs->length; ++i) {
		struct output_state *output = registry->outputs->items[i];
		struct window *window = window_setup(registry, output->width, output->height, false);
		if (!window) {
			sway_abort("Failed to create surfaces.");
		}
		lock_set_lock_surface(registry->swaylock, output->output, window->surface);
		list_add(surfaces, window);
	}
	return 0;
}
