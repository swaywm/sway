#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <sys/wait.h>
#include <signal.h>
#include <getopt.h>
#include "layout.h"
#include "config.h"
#include "log.h"
#include "handlers.h"

static void sigchld_handle(int signal);

int main(int argc, char **argv) {
	static int verbose = 0, debug = 0, validate = 0;

	static struct option long_options[] = {
		{"config", required_argument, NULL, 'c'},
		{"validate", no_argument, &validate, 1},
		{"debug", no_argument, &debug, 1},
		{"version", no_argument, NULL, 'v'},
		{"verbose", no_argument, &verbose, 1},
		{"get-socketpath", no_argument, NULL, 'p'},
	};

	/* Signal handling */
	signal(SIGCHLD, sigchld_handle);

	setenv("WLC_DIM", "0", 0);

	FILE *devnull = fopen("/dev/null", "w");
	if (devnull) {
		// NOTE: Does not work, see wlc issue #54
		wlc_set_log_file(devnull);
	}

	/* Changing code earlier than this point requires detailed review */
	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}

	char *config_path = NULL;

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "CdvVpc:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 0: // Flag
			break;
		case 'c': // config
			config_path = strdup(optarg);
			break;
		case 'C': // validate
			validate = 1;
			break;
		case 'd': // debug
			debug = 1;
			break;
		case 'v': // version
			// todo
			exit(0);
			break;
		case 'V': // verbose
			verbose = 1;
			break;
		case 'p': // --get-socketpath
			// TODO
			break;
		}
	}

	if (debug) {
		init_log(L_DEBUG);
		wlc_set_log_file(stderr);
		fclose(devnull);
		devnull = NULL;
	} else if (verbose || validate) {
		init_log(L_INFO);
	} else {
		init_log(L_ERROR);
	}

	if (validate) {
		bool valid = load_config(config_path);
		return valid ? 0 : 1;
	}

	init_layout();

	if (!load_config(config_path)) {
		sway_log(L_ERROR, "Error(s) loading config!");
	}
	if (config_path) {
		free(config_path);
	}

	wlc_run();
	if (devnull) {
		fclose(devnull);
	}

	return 0;
}

static void sigchld_handle(int signal) {
	(void) signal;
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
}
