#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/render/color.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "stringop.h"

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
		size_t to_read = (size_t)s - nread;
		ssize_t r = read(fd, b + nread, to_read);
		if ((r == -1 && errno != EINTR) || r == 0) {
			goto fail;
		}
		nread += (size_t)r;
	}
	close(fd);
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

	enum color_profile new_mode = COLOR_PROFILE_TRANSFORM;
	if (argc >= 2 && strcmp(*argv, "--device-primaries") == 0) {
		new_mode = COLOR_PROFILE_TRANSFORM_WITH_DEVICE_PRIMARIES;
		argc--;
		argv++;
	}

	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing color_profile first argument.");
	}

	if (strcmp(*argv, "gamma22") == 0) {
		wlr_color_transform_unref(config->handler_context.output_config->color_transform);
		config->handler_context.output_config->color_transform = NULL;
		config->handler_context.output_config->color_profile = new_mode;

		config->handler_context.leftovers.argc = argc - 1;
		config->handler_context.leftovers.argv = argv + 1;
	} else if (strcmp(*argv, "srgb") == 0) {
		wlr_color_transform_unref(config->handler_context.output_config->color_transform);
		config->handler_context.output_config->color_transform =
			wlr_color_transform_init_linear_to_inverse_eotf(WLR_COLOR_TRANSFER_FUNCTION_SRGB);
		config->handler_context.output_config->color_profile = new_mode;

		config->handler_context.leftovers.argc = argc - 1;
		config->handler_context.leftovers.argv = argv + 1;
	} else if (strcmp(*argv, "icc") == 0) {
		if (argc < 2) {
			return cmd_results_new(CMD_INVALID,
				"Invalid color profile specification: icc type requires a file");
		}
		if (new_mode != COLOR_PROFILE_TRANSFORM) {
			return cmd_results_new(CMD_INVALID,
				"Invalid color profile specification: --device-primaries cannot be used with icc");
		}

		char *icc_path = strdup(argv[1]);
		if (!expand_path(&icc_path)) {
			struct cmd_results *cmd_res = cmd_results_new(CMD_INVALID,
				"Invalid color profile specification: invalid file path");
			free(icc_path);
			return cmd_res;
		}

		void *data = NULL;
		size_t size = 0;
		if (!read_file_into_buf(icc_path, &data, &size)) {
			free(icc_path);
			return cmd_results_new(CMD_FAILURE,
				"Failed to load color profile: could not read ICC file");
		}
		free(icc_path);

		struct wlr_color_transform *tmp =
			wlr_color_transform_init_linear_to_icc(data, size);
		if (!tmp) {
			free(data);
			return cmd_results_new(CMD_FAILURE,
				"Failed to load color profile: failed to initialize transform from ICC");
		}
		free(data);

		wlr_color_transform_unref(config->handler_context.output_config->color_transform);
		config->handler_context.output_config->color_transform = tmp;
		config->handler_context.output_config->color_profile = COLOR_PROFILE_TRANSFORM;

		config->handler_context.leftovers.argc = argc - 2;
		config->handler_context.leftovers.argv = argv + 2;
	} else {
		return cmd_results_new(CMD_INVALID,
			"Invalid color profile specification: "
			"first argument should be gamma22|icc|srgb");
	}

	return NULL;
}
