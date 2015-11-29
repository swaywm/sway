#include <stdio.h>
#include <stdlib.h>
#include "client/registry.h"
#include "client/window.h"
#include "client/pango.h"
#include "log.h"

#define MARGIN 5

struct box_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
};

struct colors {
	uint32_t background;
	uint32_t statusline;
	uint32_t seperator;

	struct box_colors focused_workspace;
	struct box_colors active_workspace;
	struct box_colors inactive_workspace;
	struct box_colors urgent_workspace;
	struct box_colors binding_mode;
};

struct registry *registry;
struct window *window;
struct colors colors = {
	.background = 0x000000FF,
	.statusline = 0xFFFFFFFF,
	.seperator = 0x666666FF,

	.focused_workspace = {
		.border = 0x4C7899FF,
		.background = 0x285577FF,
		.text = 0xFFFFFFFF
	},
	.active_workspace = {
		.border = 0x333333FF,
		.background = 0x5F676AFF,
		.text = 0xFFFFFFFF
	},
	.inactive_workspace = {
		.border = 0x333333FF,
		.background = 0x222222FF,
		.text = 0x888888FF
	},
	.urgent_workspace = {
		.border = 0x2F343AFF,
		.background = 0x900000FF,
		.text = 0xFFFFFFFF
	},
	.binding_mode = {
		.border = 0x2F343AFF,
		.background = 0x900000FF,
		.text = 0xFFFFFFFF
	},
};

void sway_terminate(void) {
	window_teardown(window);
	registry_teardown(registry);
	exit(EXIT_FAILURE);
}

void cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo, 
			((color & 0xFF000000) >> 24) / 256.0,
			((color & 0xFF0000) >> 16) / 256.0,
			((color & 0xFF00) >> 8) / 256.0,
			(color & 0xFF) / 256.0);
}

void render() {
	cairo_save(window->cairo);
	cairo_set_operator(window->cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(window->cairo);
	cairo_restore(window->cairo);

	cairo_set_source_u32(window->cairo, colors.background);
	cairo_paint(window->cairo);

	cairo_move_to(window->cairo, MARGIN, MARGIN);
	cairo_set_source_u32(window->cairo, colors.statusline);
	pango_printf(window, "TODO: finish bar");
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	registry = registry_poll();

	if (!registry->desktop_shell) {
		sway_abort("swaybar requires the compositor to support the desktop-shell extension.");
	}

	int desired_output = atoi(argv[1]);
	sway_log(L_INFO, "Using output %d of %d", desired_output, registry->outputs->length);

	struct output_state *output = registry->outputs->items[desired_output];
	window = window_setup(registry, output->width, 30, false);
	if (!window) {
		sway_abort("Failed to create window.");
	}
	desktop_shell_set_panel(registry->desktop_shell, output->output, window->surface);
	desktop_shell_set_panel_position(registry->desktop_shell, DESKTOP_SHELL_PANEL_POSITION_BOTTOM);

	int width, height;
	get_text_size(window, &width, &height, "Test string for measuring purposes");
	window->height = height + MARGIN * 2;

	do {
		if (window_prerender(window) && window->cairo) {
			render();
			window_render(window);
		}
	} while (wl_display_dispatch(registry->display) != -1);

	window_teardown(window);
	registry_teardown(registry);

	return 0;
}
