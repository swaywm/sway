#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include "sway/extensions.h"
#include "sway/layout.h"
#include "sway/config.h"
#include "sway/security.h"
#include "sway/handlers.h"
#include "sway/input.h"
#include "sway/ipc-server.h"
#include "ipc-client.h"
#include "readline.h"
#include "stringop.h"
#include "sway.h"
#include "log.h"

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
			fprintf(stderr, "\x1B[1;31mYes, they STILL don't work with the newly announced wayland \"support\".\x1B[0m\n");
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

static void log_env() {
	const char *log_vars[] = {
		"PATH",
		"LD_LOAD_PATH",
		"LD_PRELOAD_PATH",
		"LD_LIBRARY_PATH",
		"SWAY_CURSOR_THEME",
		"SWAY_CURSOR_SIZE",
		"SWAYSOCK",
		"WLC_DRM_DEVICE",
		"WLC_SHM",
		"WLC_OUTPUTS",
		"WLC_XWAYLAND",
		"WLC_LIBINPUT",
		"WLC_REPEAT_DELAY",
		"WLC_REPEAT_RATE",
		"XKB_DEFAULT_RULES",
		"XKB_DEFAULT_MODEL",
		"XKB_DEFAULT_LAYOUT",
		"XKB_DEFAULT_VARIANT",
		"XKB_DEFAULT_OPTIONS",
	};
	for (size_t i = 0; i < sizeof(log_vars) / sizeof(char *); ++i) {
		sway_log(L_INFO, "%s=%s", log_vars[i], getenv(log_vars[i]));
	}
}

static void log_distro() {
	const char *paths[] = {
		"/etc/lsb-release",
		"/etc/os-release",
		"/etc/debian_version",
		"/etc/redhat-release",
		"/etc/gentoo-release",
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(char *); ++i) {
		FILE *f = fopen(paths[i], "r");
		if (f) {
			sway_log(L_INFO, "Contents of %s:", paths[i]);
			while (!feof(f)) {
				char *line = read_line(f);
				if (*line) {
					sway_log(L_INFO, "%s", line);
				}
				free(line);
			}
			fclose(f);
		}
	}
}

static void log_kernel() {
	FILE *f = popen("uname -a", "r");
	if (!f) {
		sway_log(L_INFO, "Unable to determine kernel version");
		return;
	}
	while (!feof(f)) {
		char *line = read_line(f);
		if (*line) {
			sway_log(L_INFO, "%s", line);
		}
		free(line);
	}
	fclose(f);
}

static void security_sanity_check() {
	// TODO: Notify users visually if this has issues
	struct stat s;
	if (stat("/proc", &s)) {
		sway_log(L_ERROR,
			"!! DANGER !! /proc is not available - sway CANNOT enforce security rules!");
	}
	if (!stat(SYSCONFDIR "/sway", &s)) {
		if (s.st_uid != 0 || s.st_gid != 0
				|| (s.st_mode & S_IWGRP) || (s.st_mode & S_IWOTH)) {
			sway_log(L_ERROR,
				"!! DANGER !! " SYSCONFDIR "/sway is not secure! It should be owned by root and set to 0755 at the minimum");
		}
	}
	struct {
		char *command;
		enum command_context context;
		bool checked;
	} expected[] = {
		{ "reload", CONTEXT_BINDING, false },
		{ "restart", CONTEXT_BINDING, false },
		{ "permit", CONTEXT_CONFIG, false },
		{ "reject", CONTEXT_CONFIG, false },
		{ "ipc", CONTEXT_CONFIG, false },
	};
	int expected_len = 5;
	for (int i = 0; i < config->command_policies->length; ++i) {
		struct command_policy *policy = config->command_policies->items[i];
		for (int j = 0; j < expected_len; ++j) {
			if (strcmp(expected[j].command, policy->command) == 0) {
				expected[j].checked = true;
				if (expected[j].context != policy->context) {
					sway_log(L_ERROR,
						"!! DANGER !! Command security policy for %s should be set to %s",
						expected[j].command, command_policy_str(expected[j].context));
				}
			}
		}
	}
	for (int j = 0; j < expected_len; ++j) {
		if (!expected[j].checked) {
			sway_log(L_ERROR,
				"!! DANGER !! Command security policy for %s should be set to %s",
				expected[j].command, command_policy_str(expected[j].context));
		}
	}
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
			if (setgid(getgid()) != 0) {
				sway_log(L_ERROR, "Unable to drop root");
				exit(EXIT_FAILURE);
			}
			if (setuid(getuid()) != 0) {
				sway_log(L_ERROR, "Unable to drop root");
				exit(EXIT_FAILURE);
			}
		}
		if (setuid(0) != -1) {
			sway_log(L_ERROR, "Root privileges can be restored.");
			exit(EXIT_FAILURE);
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
	wlc_log_set_handler(wlc_log_handler);
	detect_proprietary();

	input_devices = create_list();

	/* Changing code earlier than this point requires detailed review */
	/* (That code runs as root on systems without logind, and wlc_init drops to
	 * another user.) */
	register_wlc_handlers();
	if (!wlc_init()) {
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
	log_kernel();
	log_distro();
	log_env();

	init_layout();

	ipc_init();

	if (validate) {
		bool valid = load_main_config(config_path, false);
		return valid ? 0 : 1;
	}

	if (!load_main_config(config_path, false)) {
		sway_terminate(EXIT_FAILURE);
	}

	if (config_path) {
		free(config_path);
	}

	security_sanity_check();

	if (!terminate_request) {
		wlc_run();
	}

	list_free(input_devices);

	ipc_terminate();

	if (config) {
		free_config(config);
	}

	return exit_value;
}

