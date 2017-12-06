#define _XOPEN_SOURCE 700
#include <string.h>
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include "sway/config.h"
#include "sway/output.h"
#include "log.h"

int output_name_cmp(const void *item, const void *data) {
	const struct output_config *output = item;
	const char *name = data;

	return strcmp(output->name, name);
}

void merge_output_config(struct output_config *dst, struct output_config *src) {
	if (src->name) {
		if (dst->name) {
			free(dst->name);
		}
		dst->name = strdup(src->name);
	}
	if (src->enabled != -1) {
		dst->enabled = src->enabled;
	}
	if (src->width != -1) {
		dst->width = src->width;
	}
	if (src->height != -1) {
		dst->height = src->height;
	}
	if (src->x != -1) {
		dst->x = src->x;
	}
	if (src->y != -1) {
		dst->y = src->y;
	}
	if (src->scale != -1) {
		dst->scale = src->scale;
	}
	if (src->background) {
		if (dst->background) {
			free(dst->background);
		}
		dst->background = strdup(src->background);
	}
	if (src->background_option) {
		if (dst->background_option) {
			free(dst->background_option);
		}
		dst->background_option = strdup(src->background_option);
	}
}

static void set_mode(struct wlr_output *output, int width, int height,
		float refresh_rate) {
	struct wlr_output_mode *mode, *best = NULL;
	int mhz = (int)(refresh_rate * 1000);
	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width == width && mode->height == height) {
			if (mode->refresh == mhz) {
				best = mode;
				break;
			}
			best = mode;
		}
	}
	if (!best) {
		sway_log(L_ERROR, "Configured mode for %s not available", output->name);
	} else {
		sway_log(L_DEBUG, "Assigning configured mode to %s", output->name);
		wlr_output_set_mode(output, best);
	}
}

void apply_output_config(struct output_config *oc, swayc_t *output) {
	assert(output->type == C_OUTPUT);

	if (oc && oc->enabled == 0) {
		destroy_output(output);
		return;
	}

	struct wlr_output *wlr_output = output->sway_output->wlr_output;
	if (oc && oc->width > 0 && oc->height > 0) {
		set_mode(wlr_output, oc->width, oc->height, oc->refresh_rate);
	}
	if (oc && oc->scale > 0) {
		wlr_output->scale = oc->scale;
	}
	if (oc && oc->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
		wlr_output_transform(wlr_output, oc->transform);
	}

	// Find position for it
	if (oc && oc->x != -1 && oc->y != -1) {
		sway_log(L_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		output->x = oc->x;
		output->y = oc->y;
	} else {
		int x = 0;
		for (int i = 0; i < root_container.children->length; ++i) {
			swayc_t *c = root_container.children->items[i];
			if (c->type == C_OUTPUT) {
				if (c->width + c->x > x) {
					x = c->width + c->x;
				}
			}
		}
		output->x = x;
	}

	if (!oc || !oc->background) {
		// Look for a * config for background
		int i = list_seq_find(config->output_configs, output_name_cmp, "*");
		if (i >= 0) {
			oc = config->output_configs->items[i];
		} else {
			oc = NULL;
		}
	}

	int output_i;
	for (output_i = 0; output_i < root_container.children->length; ++output_i) {
		if (root_container.children->items[output_i] == output) {
			break;
		}
	}

	if (oc && oc->background) {
		// TODO: swaybg
		/*if (output->bg_pid != 0) {
			terminate_swaybg(output->bg_pid);
		}

		sway_log(L_DEBUG, "Setting background for output %d to %s", output_i, oc->background);

		size_t bufsize = 12;
		char output_id[bufsize];
		snprintf(output_id, bufsize, "%d", output_i);
		output_id[bufsize-1] = 0;

		char *const cmd[] = {
			"swaybg",
			output_id,
			oc->background,
			oc->background_option,
			NULL,
		};

		output->bg_pid = fork();
		if (output->bg_pid == 0) {
			execvp(cmd[0], cmd);
		}*/
	}
}

void free_output_config(struct output_config *oc) {
	if (!oc) {
		return;
	}
	free(oc->name);
	free(oc->background);
	free(oc->background_option);
	free(oc);
}
