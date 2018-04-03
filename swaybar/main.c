#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <wlr/util/log.h>
#include "config.h"
#include "swaybar/bar.h"
#include "ipc-client.h"

static struct swaybar swaybar;

void sig_handler(int signal) {
	bar_teardown(&swaybar);
	exit(0);
}

void sway_terminate(int code) {
	bar_teardown(&swaybar);
	exit(code);
}

int main(int argc, char **argv) {
	char *socket_path = NULL;
	char *bar_id = NULL;
	bool debug = false;

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"bar_id", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaybar [options...]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -v, --version          Show the version number and quit.\n"
		"  -s, --socket <socket>  Connect to sway via socket.\n"
		"  -b, --bar_id <id>      Bar ID for which to get the configuration.\n"
		"  -d, --debug            Enable debugging.\n"
		"\n"
		" PLEASE NOTE that swaybar will be automatically started by sway as\n"
		" soon as there is a 'bar' configuration block in your config file.\n"
		" You should never need to start it manually.\n";

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hvs:b:d", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 'b': // Type
			bar_id = strdup(optarg);
			break;
		case 'v':
			fprintf(stdout, "sway version " SWAY_VERSION "\n");
			exit(EXIT_SUCCESS);
			break;
		case 'd': // Debug
			debug = true;
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	if (debug) {
		wlr_log_init(L_DEBUG, NULL);
	} else {
		wlr_log_init(L_ERROR, NULL);
	}

	if (!bar_id) {
		wlr_log(L_ERROR, "No bar_id passed. "
				"Provide --bar_id or let sway start swaybar");
		return 1;
	}

	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			wlr_log(L_ERROR, "Unable to retrieve socket path");
			return 1;
		}
	}

	signal(SIGTERM, sig_handler);

	bar_setup(&swaybar, socket_path, bar_id);

	free(socket_path);
	free(bar_id);

	bar_run(&swaybar);
	bar_teardown(&swaybar);
	return 0;
}
