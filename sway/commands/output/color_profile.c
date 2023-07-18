#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/render/color.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"

static bool read_file_into_buf(const char *path, void **buf, size_t *size) {
	/* Why not use fopen/fread directly? glibc will succesfully open directories,
	 * not just files, and supports seeking on them. Instead, we directly
	 * work with file descriptors and use the more consistent open/fstat/read. */
	int fd = open(path, O_RDONLY | O_NOCTTY | O_CLOEXEC);
	if (fd == -1) {
		return false;
	}
	char *b = NULL;
	struct stat info;
	if (fstat(fd, &info) == -1) {
		goto fail;
	}
	// only regular files, to avoid issues with e.g. opening pipes
	if (!S_ISREG(info.st_mode)) {
		goto fail;
	}
	off_t s = info.st_size;
	if (s <= 0) {
		goto fail;
	}
	b = calloc(1, s);
	if (!b) {
		goto fail;
	}
	size_t nread = 0;
	while (nread < (size_t)s) {
		size_t to_read = (size_t)s  - nread;
		ssize_t r = read(fd, (void *)(b + nread), to_read);
		if ((r == -1 && errno != EINTR) || r == 0) {
			goto fail;
		}
		nread += (size_t)r;
	}
	*buf = b;
	*size = (size_t)s;
	return true; // success
fail:
	free(b);
	close(fd);
	return false;
}

struct cmd_results *output_cmd_color_profile(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing color profile first argument.");
	}

	if (strcmp(*argv, "srgb") == 0) {
		if (config->handler_context.output_config->has_color_transform == 1) {
			wlr_color_transform_unref(config->handler_context.output_config->color_transform);
		}
		config->handler_context.output_config->color_transform = NULL;
		config->handler_context.output_config->has_color_transform = 0;

		config->handler_context.leftovers.argc = argc - 1;
		config->handler_context.leftovers.argv = argv + 1;
	} else if (strcmp(*argv, "icc") == 0) {
		if (argc < 2) {
			return cmd_results_new(CMD_INVALID,
				"Invalid color profile specification; icc type requires a file");
		}
		void *data = NULL;
		size_t size = 0;
		if (!read_file_into_buf(argv[1], &data, &size)) {
			return cmd_results_new(CMD_INVALID,
				"Failed to read color profile ICC file");
		}

		struct wlr_color_transform *tmp = wlr_color_transform_init_linear_to_icc(data, size);
		if (!tmp) {
			free(data);
			return cmd_results_new(CMD_INVALID,
				"Invalid color profile specification; failed to initialize transform from ICC");
		}
		free(data);

		if (config->handler_context.output_config->has_color_transform == 1) {
			wlr_color_transform_unref(config->handler_context.output_config->color_transform);
		}
		config->handler_context.output_config->color_transform = tmp;
		config->handler_context.output_config->has_color_transform = 1;

		sway_log(SWAY_ERROR, "ICC profile loaded and set up");

		config->handler_context.leftovers.argc = argc - 2;
		config->handler_context.leftovers.argv = argv + 2;
	} else {
		return cmd_results_new(CMD_INVALID,
			"Invalid color profile specification; first argument should be icc|srgb");
	}

	return NULL;
}

