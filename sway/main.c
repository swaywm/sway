#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <sys/wait.h>
#include <signal.h>
#include "layout.h"
#include "config.h"
#include "log.h"
#include "handlers.h"

static void sigchld_handle(int signal);

int main(int argc, char **argv) {
	init_log(L_DEBUG); // TODO: Control this with command line arg
	init_layout();

	/* Signal handling */
	signal(SIGCHLD, sigchld_handle);

	if (!load_config()) {
		sway_log(L_ERROR, "Errors loading config!");
	}

	setenv("WLC_DIM", "0", 0);
	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}
	setenv("DISPLAY", ":1", 1);

	wlc_run();

	return 0;
}

static void sigchld_handle(int signal) {
	(void) signal;
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
}
