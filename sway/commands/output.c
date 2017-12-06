#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wordexp.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

static char *bg_options[] = {
	"stretch",
	"center",
	"fill",
	"fit",
	"tile",
};

struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char *name = argv[0];

	struct output_config *output = calloc(1, sizeof(struct output_config));
	if (!output) {
		return cmd_results_new(CMD_FAILURE, "output", "Unable to allocate output config");
	}
	output->x = output->y = output->width = output->height = -1;
	output->name = strdup(name);
	output->enabled = -1;
	output->scale = 1;

	// TODO: atoi doesn't handle invalid numbers

	int i;
	for (i = 1; i < argc; ++i) {
		const char *command = argv[i];

		if (strcasecmp(command, "disable") == 0) {
			output->enabled = 0;
		} else if (strcasecmp(command, "resolution") == 0 || strcasecmp(command, "res") == 0) {
			if (++i >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing resolution argument.");
				goto fail;
			}
			char *res = argv[i];
			char *x = strchr(res, 'x');
			int width = -1, height = -1;
			if (x != NULL) {
				// Format is 1234x4321
				*x = '\0';
				width = atoi(res);
				height = atoi(x + 1);
				*x = 'x';
			} else {
				// Format is 1234 4321
				width = atoi(res);
				if (++i >= argc) {
					error = cmd_results_new(CMD_INVALID, "output", "Missing resolution argument (height).");
					goto fail;
				}
				res = argv[i];
				height = atoi(res);
			}
			output->width = width;
			output->height = height;
		} else if (strcasecmp(command, "refresh_rate") == 0) {
			if (++i >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing refresh_rate argument.");
				goto fail;
			}
			output->refresh_rate = atof(argv[i]);
		} else if (strcasecmp(command, "position") == 0 || strcasecmp(command, "pos") == 0) {
			if (++i >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing position argument.");
				goto fail;
			}
			char *res = argv[i];
			char *c = strchr(res, ',');
			int x = -1, y = -1;
			if (c != NULL) {
				// Format is 1234,4321
				*c = '\0';
				x = atoi(res);
				y = atoi(c + 1);
				*c = ',';
			} else {
				// Format is 1234 4321
				x = atoi(res);
				if (++i >= argc) {
					error = cmd_results_new(CMD_INVALID, "output", "Missing position argument (y).");
					goto fail;
				}
				res = argv[i];
				y = atoi(res);
			}
			output->x = x;
			output->y = y;
		} else if (strcasecmp(command, "scale") == 0) {
			if (++i >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing scale parameter.");
				goto fail;
			}
			output->scale = atoi(argv[i]);
		} else if (strcasecmp(command, "transform") == 0) {
			if (++i >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing transform parameter.");
				goto fail;
			}
			char *value = argv[i];
			if (strcmp(value, "normal") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_NORMAL;
			} else if (strcmp(value, "90") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_90;
			} else if (strcmp(value, "180") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_180;
			} else if (strcmp(value, "270") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_270;
			} else if (strcmp(value, "flipped") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_FLIPPED;
			} else if (strcmp(value, "flipped-90") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_FLIPPED_90;
			} else if (strcmp(value, "flipped-180") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_FLIPPED_180;
			} else if (strcmp(value, "flipped-270") == 0) {
				output->transform = WL_OUTPUT_TRANSFORM_FLIPPED_270;
			} else {
				error = cmd_results_new(CMD_INVALID, "output", "Invalid output transform.");
				goto fail;
			}
		} else if (strcasecmp(command, "background") == 0 || strcasecmp(command, "bg") == 0) {
			wordexp_t p;
			if (++i >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing background file or color specification.");
				goto fail;
			}
			if (i + 1 >= argc) {
				error = cmd_results_new(CMD_INVALID, "output", "Missing background scaling mode or `solid_color`.");
				goto fail;
			}
			if (strcasecmp(argv[i + 1], "solid_color") == 0) {
				output->background = strdup(argv[argc - 2]);
				output->background_option = strdup("solid_color");
			} else {
				// argv[i+j]=bg_option
				bool valid = false;
				char *mode;
				size_t j;
				for (j = 0; j < (size_t) (argc - i); ++j) {
					mode = argv[i + j];
					for (size_t k = 0; k < sizeof(bg_options) / sizeof(char *); ++k) {
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
					error = cmd_results_new(CMD_INVALID, "output", "Missing background scaling mode.");
					goto fail;
				}

				char *src = join_args(argv + i, j);
				if (wordexp(src, &p, 0) != 0 || p.we_wordv[0] == NULL) {
					error = cmd_results_new(CMD_INVALID, "output", "Invalid syntax (%s)", src);
					goto fail;
				}
				free(src);
				src = p.we_wordv[0];
				if (config->reading && *src != '/') {
					char *conf = strdup(config->current_config);
					if (conf) {
						char *conf_path = dirname(conf);
						src = malloc(strlen(conf_path) + strlen(src) + 2);
						if (src) {
							sprintf(src, "%s/%s", conf_path, p.we_wordv[0]);
						} else {
							sway_log(L_ERROR, "Unable to allocate background source");
						}
						free(conf);
					} else {
						sway_log(L_ERROR, "Unable to allocate background source");
					}
				}
				if (!src || access(src, F_OK) == -1) {
					error = cmd_results_new(CMD_INVALID, "output", "Background file unreadable (%s)", src);
					wordfree(&p);
					goto fail;
				}

				output->background = strdup(src);
				output->background_option = strdup(mode);
				if (src != p.we_wordv[0]) {
					free(src);
				}
				wordfree(&p);

				i += j;
			}
		}
	}

	i = list_seq_find(config->output_configs, output_name_cmp, name);
	if (i >= 0) {
		// merge existing config
		struct output_config *oc = config->output_configs->items[i];
		merge_output_config(oc, output);
		free_output_config(output);
		output = oc;
	} else {
		list_add(config->output_configs, output);
	}

	sway_log(L_DEBUG, "Config stored for output %s (enabled:%d) (%d x %d @ "
			"%d, %d scale %d transform %d refresh_rate %f) (bg %s %s)",
			output->name, output->enabled, output->width,
			output->height, output->x, output->y, output->scale,
			output->transform, output->refresh_rate,
			output->background, output->background_option);

	if (output->name) {
		// Try to find the output container and apply configuration now. If
		// this is during startup then there will be no container and config
		// will be applied during normal "new output" event from wlc.
		swayc_t *cont = NULL;
		for (int i = 0; i < root_container.children->length; ++i) {
			cont = root_container.children->items[i];
			if (cont->name && ((strcmp(cont->name, output->name) == 0) || (strcmp(output->name, "*") == 0))) {
				apply_output_config(output, cont);

				if (strcmp(output->name, "*") != 0) {
					// stop looking if the output config isn't applicable to all outputs
					break;
				}
			}
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);

fail:
	free_output_config(output);
	return error;
}
