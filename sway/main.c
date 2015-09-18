#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>
#include "layout.h"
#include "config.h"
#include "log.h"
#include "readline.h"
#include "handlers.h"
#include "ipc.h"
#include "sway.h"

static bool terminate_request = false;

void sway_terminate(void) {
	terminate_request = true;
	wlc_terminate();
}

static void sigchld_handle(int signal);

static void wlc_log_handler(enum wlc_log_type type, const char *str) {
	if (type == WLC_LOG_ERROR) {
		sway_log(L_ERROR, "[wlc] %s", str);
	} else if (type == WLC_LOG_WARN) {
		sway_log(L_INFO, "[wlc] %s", str);
	} else {
		sway_log(L_DEBUG, "[wlc] %s", str);
	}
}

void detect_nvidia() {
	FILE *f = fopen("/proc/modules", "r");
	if (!f) {
		return;
	}
	while (!feof(f)) {
		char *line = read_line(f);
		if (strstr(line, "nvidia")) {
			fprintf(stderr, "\x1B[1;31mWarning: Proprietary nvidia drivers do NOT support Wayland. Use nouveau.\x1B[0m\n");
			free(line);
			break;
		}
		free(line);
	}
	fclose(f);
}

int main(int argc, char **argv) {
	static int verbose = 0, debug = 0, validate = 0;

	static struct option long_options[] = {
		{"config", required_argument, NULL, 'c'},
		{"validate", no_argument, &validate, 1},
		{"debug", no_argument, &debug, 1},
		{"version", no_argument, NULL, 'v'},
		{"verbose", no_argument, &verbose, 1},
		{"get-socketpath", no_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	/* Signal handling */
	signal(SIGCHLD, sigchld_handle);

	setenv("WLC_DIM", "0", 0);

	wlc_log_set_handler(wlc_log_handler);

	detect_nvidia();

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
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "sway version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version not detected\n");
#endif
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
	} else if (verbose || validate) {
		init_log(L_INFO);
	} else {
		init_log(L_ERROR);
	}

#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
	sway_log(L_INFO, "Starting sway version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#endif

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

	ipc_init();

	if (!terminate_request) {
		wlc_run();
	}

	ipc_terminate();

	return 0;
}

void sigchld_handle(int signal) {
	(void) signal;
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
}
