#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <json-c/json.h>
#include "log.h"
#include "ipc-client.h"
#include "util.h"

void sway_terminate(int exit_code) {
	exit(exit_code);
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

		free(pixels - 9);
		clock_gettime(CLOCK_MONOTONIC, &finish);
		ts.tv_nsec = ns;
		double fts = (double)finish.tv_sec + 1.0e-9*finish.tv_nsec;
		double sts = (double)start.tv_sec + 1.0e-9*start.tv_nsec;
		long diff = (fts - sts) * 1000000000;
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

char *get_focused_output(int socketfd) {
	uint32_t len = 0;
	char *res = ipc_single_command(socketfd, IPC_GET_WORKSPACES, NULL, &len);
	json_object *workspaces = json_tokener_parse(res);

	int length = json_object_array_length(workspaces);
	json_object *workspace, *focused, *json_output;
	char *output = NULL;
	int i;
	for (i = 0; i < length; ++i) {
		workspace = json_object_array_get_idx(workspaces, i);
		json_object_object_get_ex(workspace, "focused", &focused);
		if (json_object_get_boolean(focused) == TRUE) {
			json_object_object_get_ex(workspace, "output", &json_output);
			output = strdup(json_object_get_string(json_output));
			break;
		}
	}

	json_object_put(workspaces);
	free(res);
	return output;
}

char *default_filename(const char *extension) {
	int ext_len = strlen(extension);
	int len = 28 + ext_len; // format: "2015-12-17-180040_swaygrab.ext"
	char *filename = malloc(len * sizeof(char));
	time_t t = time(NULL);

	struct tm *lt = localtime(&t);
	strftime(filename, len, "%Y-%m-%d-%H%M%S_swaygrab.", lt);
	strncat(filename, extension, ext_len);

	return filename;
}

int main(int argc, char **argv) {
	static int capture = 0, raw = 0;
	char *socket_path = NULL;
	char *output = NULL;
	int framerate = 30;

	init_log(L_INFO);

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"capture", no_argument, NULL, 'c'},
		{"output", required_argument, NULL, 'o'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"raw", no_argument, NULL, 'r'},
		{"rate", required_argument, NULL, 'R'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaygrab [options] [file]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -c, --capture          Capture video.\n"
		"  -o, --output <output>  Output source.\n"
		"  -v, --version          Show the version number and quit.\n"
		"  -s, --socket <socket>  Use the specified socket.\n"
		"  -R, --rate <rate>      Specify framerate (default: 30)\n"
		"  -r, --raw              Write raw rgba data to stdout.\n";

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hco:vs:R:r", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 'r':
			raw = 1;
			break;
		case 'o': // output
			output = strdup(optarg);
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
			exit(EXIT_SUCCESS);
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			sway_abort("Unable to retrieve socket path");
		}
	}

	char *file = NULL;
	if (raw) {
		if (optind >= argc + 1) {
			sway_abort("Invalid usage. See `man swaygrab` %d %d", argc, optind);
		}
	} else if (optind < argc) {
		file = strdup(argv[optind]);
	}

	int socketfd = ipc_open_socket(socket_path);
	free(socket_path);

	if (!output) {
		output = get_focused_output(socketfd);
	}

	if (!file) {
		if (!capture) {
			file = default_filename("png");
		} else {
			file = default_filename("webm");
		}
	}

	if (!capture) {
		grab_and_apply_magick(file, output, socketfd, raw);
	} else {
		grab_and_apply_movie_magic(file, output, socketfd, raw, framerate);
	}

	free(output);
	free(file);
	close(socketfd);
	return 0;
}
