#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"
#include "config.h"
#include "log.h"
#include "handlers.h"

struct sway_config *config;

void load_config() {
	// TODO: Allow use of more config file locations
	const char *name = "/.sway/config";
	const char *home = getenv("HOME");
	char *temp = malloc(strlen(home) + strlen(name) + 1);
	strcpy(temp, home);
	strcat(temp, name);
	FILE *f = fopen(temp, "r");
	if (!f) {
		fprintf(stderr, "Unable to open %s for reading", temp);
		free(temp);
		exit(1);
	}
	free(temp);
	config = read_config(f);
	fclose(f);
}

int main(int argc, char **argv) {
	init_log(L_DEBUG); // TODO: Control this with command line arg
	init_layout();

	static struct wlc_interface interface = {
		.output = {
			.created = handle_output_created,
			.destroyed = handle_output_destroyed,
			.resolution = handle_output_resolution_change
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

	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}

	setenv("DISPLAY", ":1", 1);
	load_config();

	wlc_run();
	return 0;
}
