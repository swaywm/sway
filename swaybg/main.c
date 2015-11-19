#include "wayland-desktop-shell-client-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <time.h>
#include "client/window.h"
#include "client/registry.h"
#include "log.h"
#include "list.h"

list_t *surfaces;

struct registry *registry;

void sway_terminate(void) {
	int i;
	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	registry_teardown(registry);
	exit(1);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	surfaces = create_list();
	registry = registry_poll();

	if (argc < 2) {
		sway_abort("Usage: swaybg path/to/file.png");
	}

	if (!registry->desktop_shell) {
		sway_abort("swaybg requires the compositor to support the desktop-shell extension.");
	}

	int i;
	for (i = 0; i < registry->outputs->length; ++i) {
		struct output_state *output = registry->outputs->items[i];
		struct window *window = window_setup(registry, 100, 100, false);
		if (!window) {
			sway_abort("Failed to create surfaces.");
		}
		window->width = output->width;
		window->height = output->height;
		desktop_shell_set_background(registry->desktop_shell, output->output, window->surface);
		list_add(surfaces, window);
	}

	cairo_surface_t *image = cairo_image_surface_create_from_png(argv[1]);

	do {
		for (i = 0; i < surfaces->length; ++i) {
			struct window *window = surfaces->items[i];
			if (window_prerender(window) && window->cairo) {
				cairo_set_source_surface(window->cairo, image, 0, 0);
				cairo_rectangle(window->cairo, 0, 0, window->width, window->height);
				cairo_fill(window->cairo);

				window_render(window);
			}
		}
	} while (wl_display_dispatch(registry->display) != -1);

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	registry_teardown(registry);
	return 0;
}
