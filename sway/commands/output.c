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

static struct cmd_results *cmd_output_mode(struct output_config *output,
		int *i, int argc, char **argv) {
	if (++*i >= argc) {
		return cmd_results_new(CMD_INVALID, "output", "Missing mode argument.");
	}

	char *end;
	output->width = strtol(argv[*i], &end, 10);
	if (*end) {
		// Format is 1234x4321
		if (*end != 'x') {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid mode width.");
		}
		++end;
		output->height = strtol(end, &end, 10);
		if (*end) {
			if (*end != '@') {
				return cmd_results_new(CMD_INVALID, "output",
					"Invalid mode height.");
			}
			++end;
			output->refresh_rate = strtof(end, &end);
			if (strcasecmp("Hz", end) != 0) {
				return cmd_results_new(CMD_INVALID, "output",
					"Invalid mode refresh rate.");
			}
		}
	} else {
		// Format is 1234 4321
		if (++*i >= argc) {
			return cmd_results_new(CMD_INVALID, "output",
				"Missing mode argument (height).");
		}
		output->height = strtol(argv[*i], &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid mode height.");
		}
	}

	return NULL;
}

static struct cmd_results *cmd_output_position(struct output_config *output,
		int *i, int argc, char **argv) {
	if (++*i >= argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing position argument.");
	}

	char *end;
	output->x = strtol(argv[*i], &end, 10);
	if (*end) {
		// Format is 1234,4321
		if (*end != ',') {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid position x.");
		}
		++end;
		output->y = strtol(end, &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid position y.");
		}
	} else {
		// Format is 1234 4321 (legacy)
		if (++*i >= argc) {
			return cmd_results_new(CMD_INVALID, "output",
				"Missing position argument (y).");
		}
		output->y = strtol(argv[*i], &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid position y.");
		}
	}

	return NULL;
}

static struct cmd_results *cmd_output_scale(struct output_config *output,
		int *i, int argc, char **argv) {
	if (++*i >= argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing scale argument.");
	}

	char *end;
	output->scale = strtof(argv[*i], &end);
	if (*end) {
		return cmd_results_new(CMD_INVALID, "output", "Invalid scale.");
	}

	return NULL;
}

static struct cmd_results *cmd_output_transform(struct output_config *output,
		int *i, int argc, char **argv) {
	if (++*i >= argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing transform argument.");
	}

	char *value = argv[*i];
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
		return cmd_results_new(CMD_INVALID, "output",
			"Invalid output transform.");
	}

	return NULL;
}

static struct cmd_results *cmd_output_background(struct output_config *output,
		int *i, int argc, char **argv) {
	if (++*i >= argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing background file or color specification.");
	}
	const char *background = argv[*i];
	if (*i + 1 >= argc) {
		return cmd_results_new(CMD_INVALID, "output",
			"Missing background scaling mode or `solid_color`.");
	}
	const char *background_option = argv[*i];

	if (strcasecmp(background_option, "solid_color") == 0) {
		output->background = strdup(background);
		output->background_option = strdup("solid_color");
	} else {
		bool valid = false;
		char *mode;
		size_t j;
		for (j = 0; j < (size_t)(argc - *i); ++j) {
			mode = argv[*i + j];
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
		char *src = join_args(argv + *i, j);
		if (wordexp(src, &p, 0) != 0 || p.we_wordv[0] == NULL) {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid syntax (%s).", src);
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
					wlr_log(L_ERROR,
						"Unable to allocate background source");
				}
				free(conf);
			} else {
				wlr_log(L_ERROR, "Unable to allocate background source");
			}
		}
		if (!src || access(src, F_OK) == -1) {
			wordfree(&p);
			return cmd_results_new(CMD_INVALID, "output",
				"Background file unreadable (%s).", src);
		}

		output->background = strdup(src);
		output->background_option = strdup(mode);
		if (src != p.we_wordv[0]) {
			free(src);
		}
		wordfree(&p);

		*i += j;
	}

	return NULL;
}

struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1);
	if (error != NULL) {
		return error;
	}

	struct output_config *output = new_output_config(argv[0]);
	if (!output) {
		wlr_log(L_ERROR, "Failed to allocate output config");
		return NULL;
	}

	for (int i = 1; i < argc; ++i) {
		const char *command = argv[i];

		if (strcasecmp(command, "enable") == 0) {
			output->enabled = 1;
		} else if (strcasecmp(command, "disable") == 0) {
			output->enabled = 0;
		} else if (strcasecmp(command, "mode") == 0 ||
				strcasecmp(command, "resolution") == 0 ||
				strcasecmp(command, "res") == 0) {
			error = cmd_output_mode(output, &i, argc, argv);
		} else if (strcasecmp(command, "position") == 0 ||
				strcasecmp(command, "pos") == 0) {
			error = cmd_output_position(output, &i, argc, argv);
		} else if (strcasecmp(command, "scale") == 0) {
			error = cmd_output_scale(output, &i, argc, argv);
		} else if (strcasecmp(command, "transform") == 0) {
			error = cmd_output_transform(output, &i, argc, argv);
		} else if (strcasecmp(command, "background") == 0 ||
				strcasecmp(command, "bg") == 0) {
			error = cmd_output_background(output, &i, argc, argv);
		} else {
			error = cmd_results_new(CMD_INVALID, "output",
				"Invalid output subcommand: %s.", command);
		}

		if (error != NULL) {
			goto fail;
		}
	}

	int i = list_seq_find(config->output_configs, output_name_cmp, output->name);
	if (i >= 0) {
		// Merge existing config
		struct output_config *current = config->output_configs->items[i];
		merge_output_config(current, output);
		free_output_config(output);
		output = current;
	} else {
		list_add(config->output_configs, output);
	}

	wlr_log(L_DEBUG, "Config stored for output %s (enabled: %d) (%dx%d@%fHz "
		"position %d,%d scale %f transform %d) (bg %s %s)",
		output->name, output->enabled, output->width, output->height,
		output->refresh_rate, output->x, output->y, output->scale,
		output->transform, output->background, output->background_option);

	// Try to find the output container and apply configuration now. If
	// this is during startup then there will be no container and config
	// will be applied during normal "new output" event from wlroots.
	char identifier[128];
	bool all = strcmp(output->name, "*") == 0;
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type != C_OUTPUT) {
			continue;
		}

		output_get_identifier(identifier, sizeof(identifier), cont->sway_output);
		if (all || strcmp(cont->name, output->name) == 0 ||
				strcmp(identifier, output->name) == 0) {
			apply_output_config(output, cont);

			if (!all) {
				// Stop looking if the output config isn't applicable to all
				// outputs
				break;
			}
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);

fail:
	free_output_config(output);
	return error;
}
