#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include "log.h"
#include "ipc-client.h"

void sway_terminate(void) {
	exit(1);
}

int numlen(int n) {
	if (n >= 1000000) return 7;
	if (n >= 100000) return 6;
	if (n >= 10000) return 5;
	if (n >= 1000) return 4;
	if (n >= 100) return 3;
	if (n >= 10) return 2;
	return 1;
}

void grab_and_apply_magick(const char *file, const char *output, int socketfd) {
	uint32_t len = strlen(output);
	char *pixels = ipc_single_command(socketfd,
			IPC_SWAY_GET_PIXELS, output, &len);
	uint32_t *u32pixels = (uint32_t *)(pixels + 1);
	uint32_t width = u32pixels[0];
	uint32_t height = u32pixels[1];
	pixels += 9;

	if (width == 0 || height == 0) {
		sway_abort("Unknown output %s.", output);
	}

	const char *fmt = "convert -depth 8 -size %dx%d+0 rgba:- -flip %s";
	char *cmd = malloc(strlen(fmt) - 6 /*args*/
			+ numlen(width) + numlen(height) + strlen(file) + 1);
	sprintf(cmd, fmt, width, height, file);

	FILE *f = popen(cmd, "w");
	fwrite(pixels, 1, len, f);
	fflush(f);
	fclose(f);
	free(pixels);
	free(cmd);
}

int main(int argc, char **argv) {
	static int capture = 0;
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

	if (optind >= argc - 1) {
		sway_abort("Expected output and file on command line. See `man swaygrab`");
	}

	char *file = argv[optind + 1];
	char *output = argv[optind];
	int socketfd = ipc_open_socket(socket_path);
	free(socket_path);

	if (!capture) {
		grab_and_apply_magick(file, output, socketfd);
	} else {
		sway_abort("Capture is not yet supported");
	}

	close(socketfd);
	return 0;
}
