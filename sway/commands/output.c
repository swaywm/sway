#include <ctype.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
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
	"tile"
};

struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char *name = argv[0];

	struct output_config *output = calloc(1, sizeof(struct output_config));
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
				return cmd_results_new(CMD_INVALID, "output", "Missing resolution argument.");
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
					return cmd_results_new(CMD_INVALID, "output", "Missing resolution argument (height).");
				}
				res = argv[i];
				height = atoi(res);
			}
			output->width = width;
			output->height = height;
		} else if (strcasecmp(command, "position") == 0 || strcasecmp(command, "pos") == 0) {
			if (++i >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing position argument.");
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
					return cmd_results_new(CMD_INVALID, "output", "Missing position argument (y).");
				}
				res = argv[i];
				y = atoi(res);
			}
			output->x = x;
			output->y = y;
		} else if (strcasecmp(command, "scale") == 0) {
			if (++i >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing scale parameter.");
			}
			output->scale = atoi(argv[i]);
		} else if (strcasecmp(command, "background") == 0 || strcasecmp(command, "bg") == 0) {
			wordexp_t p;
			if (++i >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing background file or color specification.");
			}
			if (i + 1 >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing background scaling mode or `solid_color`.");
			}
			if (strcasecmp(argv[argc - 1], "solid_color") == 0) {
				output->background = strdup(argv[argc - 2]);
				output->background_option = strdup("solid_color");
			} else {
				char *src = join_args(argv + i, argc - i - 1);
				char *mode = argv[argc - 1];
				if (wordexp(src, &p, 0) != 0 || p.we_wordv[0] == NULL) {
					return cmd_results_new(CMD_INVALID, "output", "Invalid syntax (%s)", src);
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
					return cmd_results_new(CMD_INVALID, "output", "Background file unreadable (%s)", src);
				}
				for (char *m = mode; *m; ++m) *m = tolower(*m);
				// Check mode
				bool valid = false;
				size_t j;
				for (j = 0; j < sizeof(bg_options) / sizeof(char *); ++j) {
					if (strcasecmp(mode, bg_options[j]) == 0) {
						valid = true;
						break;
					}
				}
				if (!valid) {
					return cmd_results_new(CMD_INVALID, "output", "Invalid background scaling mode.");
				}
				output->background = strdup(src);
				output->background_option = strdup(mode);
				if (src != p.we_wordv[0]) {
					free(src);
				}
				wordfree(&p);
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

	sway_log(L_DEBUG, "Config stored for output %s (enabled:%d) (%d x %d @ %d, %d scale %d) (bg %s %s)",
			output->name, output->enabled, output->width,
			output->height, output->x, output->y, output->scale,
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
}
