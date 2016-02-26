#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include "extensions.h"
#include "layout.h"
#include "stringop.h"
#include "config.h"
#include "log.h"
#include "readline.h"
#include "handlers.h"
#include "ipc-client.h"
#include "ipc-server.h"
#include "input.h"
#include "sway.h"

static bool terminate_request = false;
static int exit_value = 0;

void sway_terminate(int exit_code) {
	terminate_request = true;
	exit_value = exit_code;
	wlc_terminate();
}

void sig_handler(int signal) {
	close_views(&root_container);
	sway_terminate(EXIT_SUCCESS);
}

static void wlc_log_handler(enum wlc_log_type type, const char *str) {
	if (type == WLC_LOG_ERROR) {
		sway_log(L_ERROR, "[wlc] %s", str);
	} else if (type == WLC_LOG_WARN) {
		sway_log(L_INFO, "[wlc] %s", str);
	} else {
		sway_log(L_DEBUG, "[wlc] %s", str);
	}
}

void detect_proprietary() {
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
		if (strstr(line, "fglrx")) {
			fprintf(stderr, "\x1B[1;31mWarning: Proprietary AMD drivers do NOT support Wayland. Use radeon.\x1B[0m\n");
			free(line);
			break;
		}
		free(line);
	}
	fclose(f);
}

void run_as_ipc_client(char *command, char *socket_path) {
	int socketfd = ipc_open_socket(socket_path);
	uint32_t len = strlen(command);
	char *resp = ipc_single_command(socketfd, IPC_COMMAND, command, &len);
	printf("%s\n", resp);
	close(socketfd);
}

int main(int argc, char **argv) {
	static int verbose = 0, debug = 0, validate = 0;

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"config", required_argument, NULL, 'c'},
		{"validate", no_argument, NULL, 'C'},
		{"debug", no_argument, NULL, 'd'},
		{"version", no_argument, NULL, 'v'},
		{"verbose", no_argument, NULL, 'V'},
		{"get-socketpath", no_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	char *config_path = NULL;

	const char* usage =
		"Usage: sway [options] [command]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -c, --config <config>  Specify a config file.\n"
		"  -C, --validate         Check the validity of the config file, then exit.\n"
		"  -d, --debug            Enables full logging, including debug information.\n"
		"  -v, --version          Show the version number and quit.\n"
		"  -V, --verbose          Enables more verbose logging.\n"
		"      --get-socketpath   Gets the IPC socket path and prints it, then exits.\n"
		"\n";

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hCdvVc:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'h': // help
			fprintf(stdout, "%s", usage);
			exit(EXIT_SUCCESS);
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
			exit(EXIT_SUCCESS);
			break;
		case 'V': // verbose
			verbose = 1;
			break;
		case 'p': ; // --get-socketpath
			if (getenv("SWAYSOCK")) {
				fprintf(stdout, "%s\n", getenv("SWAYSOCK"));
				exit(EXIT_SUCCESS);
			} else {
				fprintf(stderr, "sway socket not detected.\n");
				exit(EXIT_FAILURE);
			}
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) { // Behave as IPC client
		if(optind != 1) {
			sway_log(L_ERROR, "Don't use options with the IPC client");
			exit(EXIT_FAILURE);
		}
		if (getuid() != geteuid() || getgid() != getegid()) {
			if (setgid(getgid()) != 0 || setuid(getuid()) != 0) {
				sway_log(L_ERROR, "Unable to drop root");
				exit(EXIT_FAILURE);
			}
		}
		char *socket_path = getenv("SWAYSOCK");
		if (!socket_path) {
			sway_log(L_ERROR, "Unable to retrieve socket path");
			exit(EXIT_FAILURE);
		}
		char *command = join_args(argv + optind, argc - optind);
		run_as_ipc_client(command, socket_path);
		return 0;
	}

	// we need to setup logging before wlc_init in case it fails.
	if (debug) {
		init_log(L_DEBUG);
	} else if (verbose || validate) {
		init_log(L_INFO);
	} else {
		init_log(L_ERROR);
	}
	setenv("WLC_DIM", "0", 0);
	wlc_log_set_handler(wlc_log_handler);
	detect_proprietary();

	input_devices = create_list();

	/* Changing code earlier than this point requires detailed review */
	/* (That code runs as root on systems without logind, and wlc_init drops to
	 * another user.) */
	if (!wlc_init(&interface, argc, argv)) {
		return 1;
	}
	register_extensions();

	// handle SIGTERM signals
	signal(SIGTERM, sig_handler);

	// prevent ipc from crashing sway
	signal(SIGPIPE, SIG_IGN);

#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
	sway_log(L_INFO, "Starting sway version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#endif

	init_layout();

	if (validate) {
		bool valid = load_config(config_path);
		return valid ? 0 : 1;
	}

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

	if (input_devices) {
		free(input_devices);
	}

	ipc_terminate();

	return exit_value;
}

