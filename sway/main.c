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

	setenv("WLC_DIM", "0", 0);
	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}
	setenv("DISPLAY", ":1", 1);

	if (!load_config()) {
		sway_abort("Unable to load config");
	}

	wlc_run();
	return 0;
}
