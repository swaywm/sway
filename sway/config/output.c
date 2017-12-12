#define _XOPEN_SOURCE 700
#include <string.h>
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/config.h"
#include "sway/output.h"
#include "log.h"

int output_name_cmp(const void *item, const void *data) {
	const struct output_config *output = item;
	const char *name = data;

	return strcmp(output->name, name);
}

struct output_config *new_output_config() {
	struct output_config *oc = calloc(1, sizeof(struct output_config));
	if (oc == NULL) {
		return NULL;
	}
	oc->enabled = -1;
	oc->width = oc->height -1;
	oc->refresh_rate = -1;
	oc->x = oc->y = -1;
	oc->scale = -1;
	oc->transform = -1;
	return oc;
}

void merge_output_config(struct output_config *dst, struct output_config *src) {
	if (src->name) {
		free(dst->name);
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
	if (src->refresh_rate != -1) {
		dst->refresh_rate = src->refresh_rate;
	}
	if (src->transform != -1) {
		dst->transform = src->transform;
	}
	if (src->background) {
		free(dst->background);
		dst->background = strdup(src->background);
	}
	if (src->background_option) {
		free(dst->background_option);
		dst->background_option = strdup(src->background_option);
	}
}

static void set_mode(struct wlr_output *output, int width, int height,
		float refresh_rate) {
	int mhz = (int)(refresh_rate * 1000);
	if (wl_list_empty(&output->modes)) {
		sway_log(L_DEBUG, "Assigning custom mode to %s", output->name);
		wlr_output_set_custom_mode(output, width, height, mhz);
		return;
	}

	struct wlr_output_mode *mode, *best = NULL;
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

	struct wlr_output *wlr_output = output->sway_output->wlr_output;
	if (oc && oc->enabled == 0) {
		wlr_output_layout_remove(root_container.output_layout, wlr_output);
		destroy_output(output);
		return;
	}

	if (oc && oc->width > 0 && oc->height > 0) {
		sway_log(L_DEBUG, "Set %s mode to %dx%d (%f GHz)", oc->name, oc->width,
			oc->height, oc->refresh_rate);
		set_mode(wlr_output, oc->width, oc->height, oc->refresh_rate);
	}
	if (oc && oc->scale > 0) {
		sway_log(L_DEBUG, "Set %s scale to %d", oc->name, oc->scale);
		wlr_output_set_scale(wlr_output, oc->scale);
		wl_signal_emit(&output->sway_output->events.scale, output->sway_output);
	}
	if (oc && oc->transform >= 0) {
		sway_log(L_DEBUG, "Set %s transform to %d", oc->name, oc->transform);
		wlr_output_transform(wlr_output, oc->transform);
		wl_signal_emit(&output->sway_output->events.transform, output->sway_output);
	}

	// Find position for it
	if (oc && (oc->x != -1 || oc->y != -1)) {
		sway_log(L_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		wlr_output_layout_add(root_container.output_layout, wlr_output, oc->x,
			oc->y);
	} else {
		wlr_output_layout_add_auto(root_container.output_layout, wlr_output);
	}
	struct wlr_box *output_layout_box =
		wlr_output_layout_get_box(root_container.output_layout, wlr_output);
	output->x = output_layout_box->x;
	output->y = output_layout_box->y;
	output->width = output_layout_box->width;
	output->height = output_layout_box->height;

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
		// TODO swaybg
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
