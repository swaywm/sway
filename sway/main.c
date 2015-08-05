#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "config.h"

struct sway_config *config;

bool load_config() {
	// TODO: Allow use of more config file locations
	const char *name = "/.i3/config";
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
	return true;
}

int main(int argc, char **argv) {
	if (!load_config()) {
		return 0;
	}
	return 0;

	static struct wlc_interface interface = { };
	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}
	wlc_run();
	return 0;
}
