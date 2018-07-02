#define _XOPEN_SOURCE 500
#include <libgen.h>
#include <strings.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "stringop.h"

static const char *bg_options[] = {
	"stretch",
	"center",
	"fill",
	"fit",
	"tile",
};

struct cmd_results *output_cmd_background(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "output", "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing background file or color specification.");
	}
	if (argc < 2) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing background scaling mode or `solid_color`.");
	}

	struct output_config *output = config->handler_context.output_config;

	if (strcasecmp(argv[1], "solid_color") == 0) {
		output->background = calloc(1, strlen(argv[0]) + 3);
		snprintf(output->background, strlen(argv[0]) + 3, "\"%s\"", argv[0]);
		output->background_option = strdup("solid_color");
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
			return cmd_results_new(CMD_INVALID, "output",
				"Missing background scaling mode.");
		}

		wordexp_t p;
		char *src = join_args(argv, j);
		if (wordexp(src, &p, 0) != 0 || p.we_wordv[0] == NULL) {
			struct cmd_results *cmd_res = cmd_results_new(CMD_INVALID, "output",
				"Invalid syntax (%s)", src);
			free(src);
			wordfree(&p);
			return cmd_res;
		}
		free(src);
		src = strdup(p.we_wordv[0]);
		wordfree(&p);
		if (!src) {
			wlr_log(L_ERROR, "Failed to duplicate string");
			return cmd_results_new(CMD_FAILURE, "output",
				"Unable to allocate resource");
		}

		if (config->reading && *src != '/') {
			// src file is inside configuration dir

			char *conf = strdup(config->current_config);
			if (!conf) {
				wlr_log(L_ERROR, "Failed to duplicate string");
				free(src);
				return cmd_results_new(CMD_FAILURE, "output",
						"Unable to allocate resources");
			}

			char *conf_path = dirname(conf);
			char *rel_path = src;
			src = malloc(strlen(conf_path) + strlen(src) + 2);
			if (!src) {
				free(rel_path);
				free(conf);
				wlr_log(L_ERROR, "Unable to allocate memory");
				return cmd_results_new(CMD_FAILURE, "output",
						"Unable to allocate resources");
			}

			sprintf(src, "%s/%s", conf_path, rel_path);
			free(rel_path);
			free(conf);
		}

		if (access(src, F_OK) == -1) {
			struct cmd_results *cmd_res = cmd_results_new(CMD_FAILURE, "output",
				"Unable to access background file '%s': %s", src, strerror(errno));
			free(src);
			return cmd_res;
		}

		output->background = src;
		output->background_option = strdup(mode);
		argc -= j + 1; argv += j + 1;
	}

	config->handler_context.leftovers.argc = argc;
	config->handler_context.leftovers.argv = argv;
	return NULL;
}

