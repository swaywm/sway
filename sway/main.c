#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200112L
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/capability.h>
#include <sys/prctl.h>
#endif
#include "sway/config.h"
#include "sway/server.h"
#include "sway/layout.h"
#include "sway/ipc-server.h"
#include "ipc-client.h"
#include "log.h"
#include "readline.h"
#include "stringop.h"
#include "util.h"

static bool terminate_request = false;
static int exit_value = 0;
struct sway_server server;

void sway_terminate(int exit_code) {
	terminate_request = true;
	exit_value = exit_code;
	wl_display_terminate(server.wl_display);
}

void sig_handler(int signal) {
	//close_views(&root_container);
	sway_terminate(EXIT_SUCCESS);
}

void detect_raspi() {
	bool raspi = false;
	FILE *f = fopen("/sys/firmware/devicetree/base/model", "r");
	if (!f) {
		return;
	}
	char *line;
	while(!feof(f)) {
		if (!(line = read_line(f))) {
			break;
		}
		if (strstr(line, "Raspberry Pi")) {
			raspi = true;
		}
		free(line);
	}
	fclose(f);
	FILE *g = fopen("/proc/modules", "r");
	if (!g) {
		return;
	}
	bool vc4 = false;
	while (!feof(g)) {
		if (!(line = read_line(g))) {
			break;
		}
		if (strstr(line, "vc4")) {
			vc4 = true;
		}
		free(line);
	}
	fclose(g);
	if (!vc4 && raspi) {
		fprintf(stderr, "\x1B[1;31mWarning: You have a "
				"Raspberry Pi, but the vc4 Module is "
				"not loaded! Set 'dtoverlay=vc4-kms-v3d'"
				"in /boot/config.txt and reboot.\x1B[0m\n");
	}
}

void detect_proprietary() {
	FILE *f = fopen("/proc/modules", "r");
	if (!f) {
		return;
	}
	while (!feof(f)) {
		char *line;
		if (!(line = read_line(f))) {
			break;
		}
		if (strstr(line, "nvidia")) {
			fprintf(stderr, "\x1B[1;31mWarning: Proprietary Nvidia drivers are "
				"NOT supported. Use Nouveau.\x1B[0m\n");
			free(line);
			break;
		}
		if (strstr(line, "fglrx")) {
			fprintf(stderr, "\x1B[1;31mWarning: Proprietary AMD drivers do "
				"NOT support Wayland. Use radeon.\x1B[0m\n");
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
		"SWAYSOCK"
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
				char *line;
				if (!(line = read_line(f))) {
					break;
				}
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
		char *line;
		if (!(line = read_line(f))) {
			break;
		}
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
#ifdef __linux__
	cap_flag_value_t v;
	cap_t cap = cap_get_proc();
	if (!cap || cap_get_flag(cap, CAP_SYS_PTRACE, CAP_PERMITTED, &v) != 0 || v != CAP_SET) {
		sway_log(L_ERROR,
			"!! DANGER !! Sway does not have CAP_SYS_PTRACE and cannot enforce security rules for processes running as other users.");
	}
	if (cap) {
		cap_free(cap);
	}
#endif
}

static void executable_sanity_check() {
#ifdef __linux__
		struct stat sb;
		char *exe = realpath("/proc/self/exe", NULL);
		stat(exe, &sb);
		// We assume that cap_get_file returning NULL implies ENODATA
		if (sb.st_mode & (S_ISUID|S_ISGID) && cap_get_file(exe)) {
			sway_log(L_ERROR,
				"sway executable has both the s(g)uid bit AND file caps set.");
			sway_log(L_ERROR,
				"This is strongly discouraged (and completely broken).");
			sway_log(L_ERROR,
				"Please clear one of them (either the suid bit, or the file caps).");
			sway_log(L_ERROR,
				"If unsure, strip the file caps.");
			exit(EXIT_FAILURE);
		}
		free(exe);
#endif
}

static void drop_permissions(bool keep_caps) {
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
#ifdef __linux__
	if (keep_caps) {
		// Drop every cap except CAP_SYS_PTRACE
		cap_t caps = cap_init();
		cap_value_t keep = CAP_SYS_PTRACE;
		sway_log(L_INFO, "Dropping extra capabilities");
		if (cap_set_flag(caps, CAP_PERMITTED, 1, &keep, CAP_SET) ||
			cap_set_flag(caps, CAP_EFFECTIVE, 1, &keep, CAP_SET) ||
			cap_set_proc(caps)) {
			sway_log(L_ERROR, "Failed to drop extra capabilities");
			exit(EXIT_FAILURE);
		}
	}
#endif
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

	// Security:
	unsetenv("LD_PRELOAD");
#ifdef _LD_LIBRARY_PATH
	setenv("LD_LIBRARY_PATH", _LD_LIBRARY_PATH, 1);
#else
	unsetenv("LD_LIBRARY_PATH");
#endif

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
			fprintf(stdout, "sway version " SWAY_VERSION "\n");
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

	// TODO: switch logging over to wlroots?
	if (debug) {
		init_log(L_DEBUG);
	} else if (verbose || validate) {
		init_log(L_INFO);
	} else {
		init_log(L_ERROR);
	}

	if (optind < argc) { // Behave as IPC client
		if(optind != 1) {
			sway_log(L_ERROR, "Don't use options with the IPC client");
			exit(EXIT_FAILURE);
		}
		drop_permissions(false);
		char *socket_path = getenv("SWAYSOCK");
		if (!socket_path) {
			sway_log(L_ERROR, "Unable to retrieve socket path");
			exit(EXIT_FAILURE);
		}
		char *command = join_args(argv + optind, argc - optind);
		run_as_ipc_client(command, socket_path);
		return 0;
	}

	executable_sanity_check();
	bool suid = false;
#ifdef __linux__
	if (getuid() != geteuid() || getgid() != getegid()) {
		// Retain capabilities after setuid()
		if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0)) {
			sway_log(L_ERROR, "Cannot keep caps after setuid()");
			exit(EXIT_FAILURE);
		}
		suid = true;
	}
#endif

	log_kernel();
	log_distro();
	detect_proprietary();
	detect_raspi();

#ifdef __linux__
	drop_permissions(suid);
#endif
	// handle SIGTERM signals
	signal(SIGTERM, sig_handler);

	// prevent ipc from crashing sway
	signal(SIGPIPE, SIG_IGN);

	sway_log(L_INFO, "Starting sway version " SWAY_VERSION "\n");

	if (!server_init(&server)) {
		return 1;
	}

	init_layout();
	ipc_init(&server);
	log_env();

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
		server_run(&server);
	}

	server_fini(&server);

	ipc_terminate();

	if (config) {
		free_config(config);
	}

	return exit_value;
}
