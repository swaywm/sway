#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
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

void grab_and_apply_magick(const char *file, const char *output,
		int socketfd, int raw) {
	uint32_t len = strlen(output);
	char *pixels = ipc_single_command(socketfd,
			IPC_SWAY_GET_PIXELS, output, &len);
	uint32_t *u32pixels = (uint32_t *)(pixels + 1);
	uint32_t width = u32pixels[0];
	uint32_t height = u32pixels[1];
	len -= 9;
	pixels += 9;

	if (width == 0 || height == 0) {
		sway_abort("Unknown output %s.", output);
	}

	if (raw) {
		fwrite(pixels, 1, len, stdout);
		fflush(stdout);
		free(pixels - 9);
		return;
	}

	const char *fmt = "convert -depth 8 -size %dx%d+0 rgba:- -flip %s";
	char *cmd = malloc(strlen(fmt) - 6 /*args*/
			+ numlen(width) + numlen(height) + strlen(file) + 1);
	sprintf(cmd, fmt, width, height, file);

	FILE *f = popen(cmd, "w");
	fwrite(pixels, 1, len, f);
	fflush(f);
	fclose(f);
	free(pixels - 9);
	free(cmd);
}

void grab_and_apply_movie_magic(const char *file, const char *output,
		int socketfd, int raw, int framerate) {
	if (raw) {
		sway_log(L_ERROR, "Raw capture data is not yet supported. Proceeding with ffmpeg normally.");
	}

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

	const char *fmt = "ffmpeg -f rawvideo -framerate %d "
		"-video_size %dx%d -pixel_format argb "
		"-i pipe:0 -r %d -vf vflip %s";
	char *cmd = malloc(strlen(fmt) - 8 /*args*/
			+ numlen(width) + numlen(height) + numlen(framerate) * 2 
			+ strlen(file) + 1);
	sprintf(cmd, fmt, framerate, width, height, framerate, file);

	long ns = (long)(1000000000 * (1.0 / framerate));
	struct timespec start, finish, ts;
	ts.tv_sec = 0;

	FILE *f = popen(cmd, "w");
	fwrite(pixels, 1, len, f);
	free(pixels - 9);
	int sleep = 0;
	while (sleep != -1) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		len = strlen(output);
		pixels = ipc_single_command(socketfd,
				IPC_SWAY_GET_PIXELS, output, &len);
		pixels += 9;
		len -= 9;

		fwrite(pixels, 1, len, f);

		clock_gettime(CLOCK_MONOTONIC, &finish);
		ts.tv_nsec = ns;
		double fts = (double)finish.tv_sec + 1.0e-9*finish.tv_nsec;
		double sts = (double)start.tv_sec + 1.0e-9*start.tv_nsec;
		long diff = (fts - sts) * 1000000000;
		sway_log(L_INFO, "%f %f %ld", sts, fts, diff);
		ts.tv_nsec = ns - diff;
		if (ts.tv_nsec < 0) {
			ts.tv_nsec = 0;
		}
		sleep = nanosleep(&ts, NULL);
	}
	fflush(f);

	fclose(f);
	free(cmd);
}

int main(int argc, char **argv) {
	static int capture = 0, raw = 0;
	char *socket_path = NULL;
	int framerate = 30;

	init_log(L_INFO);

	static struct option long_options[] = {
		{"capture", no_argument, &capture, 'c'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"raw", no_argument, &raw, 'r'},
		{"rate", required_argument, NULL, 'R'},
		{0, 0, 0, 0}
	};

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "cvs:r", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 0: // Flag
			break;
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 'r':
			raw = 1;
			break;
		case 'c':
			capture = 1;
			break;
		case 'R': // Frame rate
			framerate = atoi(optarg);
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

	char *file = NULL, *output = NULL;
	if (raw) {
		if (optind >= argc) {
			sway_abort("Invalid usage. See `man swaygrab` %d %d", argc, optind);
		}
		output = argv[optind];
	} else {
		if (optind >= argc - 1) {
			sway_abort("Invalid usage. See `man swaygrab`");
		}
		file = argv[optind + 1];
		output = argv[optind];
	}

	int socketfd = ipc_open_socket(socket_path);
	free(socket_path);

	if (!capture) {
		grab_and_apply_magick(file, output, socketfd, raw);
	} else {
		grab_and_apply_movie_magic(file, output, socketfd, raw, framerate);
	}

	close(socketfd);
	return 0;
}
