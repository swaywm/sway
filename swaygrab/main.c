#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "ipc-client.h"

void sway_terminate(void) {
	exit(1);
}

int main(int argc, const char **argv) {
	int capture;
	char *socket_path = NULL;

	init_log(L_INFO);

	static struct option long_options[] = {
		{"capture", no_argument, &capture, 'c'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{0, 0, 0, 0}
	};

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "cvs:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 0: // Flag
			break;
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 'v':
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "sway version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version not detected\n");
#endif
			exit(0);
			break;
		}
	}

	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			sway_abort("Unable to retrieve socket path");
		}
	}

	if (optind >= argc) {
		sway_abort("Expected output file on command line. See `man swaygrab`");
	}

	char *out = argv[optind];
	int socketfd = ipc_open_socket(socket_path);
	free(socket_path);

	close(socketfd);
	free(out);
	return 0;
}
