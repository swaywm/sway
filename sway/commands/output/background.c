#define _POSIX_C_SOURCE 200809L
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/swaynag.h"
#include "log.h"
#include "stringop.h"

static const char *bg_options[] = {
	"stretch",
	"center",
	"fill",
	"fit",
	"tile",
};

static bool validate_color(const char *color) {
	if (strlen(color) != 7 || color[0] != '#') {
		return false;
	}

	char *ptr = NULL;
	strtol(&color[1], &ptr, 16);
	return *ptr == '\0';
}

struct cmd_results *output_cmd_background(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID,
			"Missing background file or color specification.");
	}
	if (argc < 2) {
		return cmd_results_new(CMD_INVALID,
			"Missing background scaling mode or `solid_color`.");
	}

	struct output_config *output = config->handler_context.output_config;

	if (strcasecmp(argv[1], "solid_color") == 0) {
		if (!validate_color(argv[0])) {
			return cmd_results_new(CMD_INVALID,
					"Colors should be of the form #RRGGBB");
		}
		output->background = strdup(argv[0]);
		output->background_option = strdup("solid_color");
		output->background_fallback = NULL;
		argc -= 2; argv += 2;
	} else {
		bool valid = false;
		char *mode;
		size_t j;
		for (j = 0; j < (size_t)argc; ++j) {
			mode = argv[j];
			size_t n = sizeof(bg_options) / sizeof(char *);
			for (size_t k = 0; k < n; ++k) {
				if (strcasecmp(mode, bg_options[k]) == 0) {
					valid = true;
					break;
				}
			}
			if (valid) {
				break;
			}
		}
		if (!valid) {
			return cmd_results_new(CMD_INVALID,
				"Missing background scaling mode.");
		}
		if (j == 0) {
			return cmd_results_new(CMD_INVALID, "Missing background file");
		}

		char *src = join_args(argv, j);
		if (!expand_path(&src)) {
			struct cmd_results *cmd_res = cmd_results_new(CMD_INVALID,
				"Invalid syntax (%s)", src);
			free(src);
			return cmd_res;
		}
		if (!src) {
			sway_log(SWAY_ERROR, "Failed to allocate expanded path");
			return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
		}

		if (config->reading && *src != '/') {
			// src file is inside configuration dir

			char *conf = strdup(config->current_config_path);
			if (!conf) {
				sway_log(SWAY_ERROR, "Failed to duplicate string");
				free(src);
				return cmd_results_new(CMD_FAILURE,
						"Unable to allocate resources");
			}

			char *conf_path = dirname(conf);
			char *real_src = malloc(strlen(conf_path) + strlen(src) + 2);
			if (!real_src) {
				free(src);
				free(conf);
				sway_log(SWAY_ERROR, "Unable to allocate memory");
				return cmd_results_new(CMD_FAILURE,
						"Unable to allocate resources");
			}

			snprintf(real_src, strlen(conf_path) + strlen(src) + 2, "%s/%s", conf_path, src);
			free(src);
			free(conf);
			src = real_src;
		}

		bool can_access = access(src, F_OK) != -1;
		if (!can_access) {
			sway_log_errno(SWAY_ERROR, "Unable to access background file '%s'",
					src);
			config_add_swaynag_warning("Unable to access background file '%s'",
					src);
			free(src);
		} else {
			output->background = src;
			output->background_option = strdup(mode);
		}
		argc -= j + 1; argv += j + 1;

		output->background_fallback = NULL;
		if (argc && *argv[0] == '#') {
			if (!validate_color(argv[0])) {
				return cmd_results_new(CMD_INVALID,
						"fallback color should be of the form #RRGGBB");
			}

			output->background_fallback = strdup(argv[0]);
			argc--; argv++;

			if (!can_access) {
				output->background = output->background_fallback;
				output->background_option = strdup("solid_color");
				output->background_fallback = NULL;
			}
		}
	}

	config->handler_context.leftovers.argc = argc;
	config->handler_context.leftovers.argv = argv;
	return NULL;
}
