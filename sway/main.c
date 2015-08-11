#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "config.h"
#include "log.h"
#include "handlers.h"


int main(int argc, char **argv) {
	init_log(L_DEBUG); // TODO: Control this with command line arg
	init_layout();

	static struct wlc_interface interface = {
		.output = {
			.created = handle_output_created,
			.destroyed = handle_output_destroyed,
			.resolution = handle_output_resolution_change,
			.focus = handle_output_focused
		},
		.view = {
			.created = handle_view_created,
			.destroyed = handle_view_destroyed,
			.focus = handle_view_focus,
			.request = {
				.geometry = handle_view_geometry_request
			}
		},
		.keyboard = {
			.key = handle_key
		},
		.pointer = {
			.motion = handle_pointer_motion,
			.button = handle_pointer_button
		}

	};

	if (!load_config()) {
		sway_abort("Unable to load config");
	}

	setenv("WLC_DIM", "0", 0);
	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}
	setenv("DISPLAY", ":1", 1);


	wlc_run();
	return 0;
}
