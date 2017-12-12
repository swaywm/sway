#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200112L
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
#ifdef __linux__
#include <sys/capability.h>
#include <sys/prctl.h>
#endif
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
#include "util.h"

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
	bool nvidia = false, nvidia_modeset = false, nvidia_uvm = false, nvidia_drm = false;
	while (!feof(f)) {
		char *line;
		if (!(line = read_line(f))) {
			break;
		}
		if (strstr(line, "nvidia")) {
			nvidia = true;
		}
		if (strstr(line, "nvidia_modeset")) {
			nvidia_modeset = true;
		}
		if (strstr(line, "nvidia_uvm")) {
			nvidia_uvm = true;
		}
		if (strstr(line, "nvidia_drm")) {
			nvidia_drm = true;
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
	if (nvidia) {
		fprintf(stderr, "\x1B[1;31mWarning: Proprietary nvidia driver support "
			"is considered experimental. Nouveau is strongly recommended."
			"\x1B[0m\n");
		if (!nvidia_modeset || !nvidia_uvm || !nvidia_drm) {
			fprintf(stderr, "\x1B[1;31mWarning: You do not have all of the "
				"necessary kernel modules loaded for nvidia support. "
				"You need nvidia, nvidia_modeset, nvidia_uvm, and nvidia_drm."
				"\x1B[0m\n");
		}
#ifdef __linux__
		f = fopen("/sys/module/nvidia_drm/parameters/modeset", "r");
		if (f) {
			char *line = read_line(f);
			if (line && strstr(line, "Y")) {
				// nvidia-drm.modeset is set to 0
				fprintf(stderr, "\x1B[1;31mWarning: You must load "
					"nvidia-drm with the modeset option on to use "
					"the proprietary driver. Consider adding "
					"nvidia-drm.modeset=1 to your kernel command line "
					"parameters.\x1B[0m\n");
			}
			fclose(f);
			free(line);
		} else {
			// nvidia-drm.modeset is not set
			fprintf(stderr, "\x1B[1;31mWarning: You must load "
				"nvidia-drm with the modeset option on to use "
				"the proprietary driver. Consider adding "
				"nvidia-drm.modeset=1 to your kernel command line "
				"parameters.\x1B[0m\n");
		}
#else
		f = fopen("/proc/cmdline", "r");
		if (f) {
			char *line = read_line(f);
			if (line && !strstr(line, "nvidia-drm.modeset=1")) {
				fprintf(stderr, "\x1B[1;31mWarning: You must add "
					"nvidia-drm.modeset=1 to your kernel command line to use "
					"the proprietary driver.\x1B[0m\n");
			}
			fclose(f);
			free(line);
		}
#endif
	}
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

	// we need to setup logging before wlc_init in case it fails.
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

	executable_sanity_check();
#ifdef __linux__
	bool suid = false;
	if (getuid() != geteuid() || getgid() != getegid()) {
		// Retain capabilities after setuid()
		if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0)) {
			sway_log(L_ERROR, "Cannot keep caps after setuid()");
			exit(EXIT_FAILURE);
		}
		suid = true;
	}
#endif

	wlc_log_set_handler(wlc_log_handler);
	log_kernel();
	log_distro();
	log_env();
	detect_proprietary();
	detect_raspi();

	input_devices = create_list();

	/* Changing code earlier than this point requires detailed review */
	/* (That code runs as root on systems without logind, and wlc_init drops to
	 * another user.) */
	register_wlc_handlers();
	if (!wlc_init()) {
		return 1;
	}
	register_extensions();

#ifdef __linux__
	if (suid) {
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
	// handle SIGTERM signals
	signal(SIGTERM, sig_handler);

	// prevent ipc from crashing sway
	signal(SIGPIPE, SIG_IGN);

	sway_log(L_INFO, "Starting sway version " SWAY_VERSION "\n");

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
