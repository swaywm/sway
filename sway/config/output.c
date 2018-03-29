#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
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

void output_get_identifier(char *identifier, size_t len,
		struct sway_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;
	snprintf(identifier, len, "%s %s %s", wlr_output->make, wlr_output->model,
		wlr_output->serial);
}

struct output_config *new_output_config(const char *name) {
	struct output_config *oc = calloc(1, sizeof(struct output_config));
	if (oc == NULL) {
		return NULL;
	}
	oc->name = strdup(name);
	if (oc->name == NULL) {
		free(oc);
		return NULL;
	}
	oc->enabled = -1;
	oc->width = oc->height = -1;
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
		wlr_log(L_DEBUG, "Assigning custom mode to %s", output->name);
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
		wlr_log(L_ERROR, "Configured mode for %s not available", output->name);
	} else {
		wlr_log(L_DEBUG, "Assigning configured mode to %s", output->name);
		wlr_output_set_mode(output, best);
	}
}

void terminate_swaybg(pid_t pid) {
	int ret = kill(pid, SIGTERM);
	if (ret != 0) {
		wlr_log(L_ERROR, "Unable to terminate swaybg [pid: %d]", pid);
	} else {
		int status;
		waitpid(pid, &status, 0);
	}
}

void apply_output_config(struct output_config *oc, swayc_t *output) {
	assert(output->type == C_OUTPUT);

	struct wlr_output *wlr_output = output->sway_output->wlr_output;
	if (oc && oc->enabled == 0) {
		wlr_output_layout_remove(root_container.sway_root->output_layout,
			wlr_output);
		destroy_output(output);
		return;
	}

	if (oc && oc->width > 0 && oc->height > 0) {
		wlr_log(L_DEBUG, "Set %s mode to %dx%d (%f GHz)", oc->name, oc->width,
			oc->height, oc->refresh_rate);
		set_mode(wlr_output, oc->width, oc->height, oc->refresh_rate);
	}
	if (oc && oc->scale > 0) {
		wlr_log(L_DEBUG, "Set %s scale to %f", oc->name, oc->scale);
		wlr_output_set_scale(wlr_output, oc->scale);
	}
	if (oc && oc->transform >= 0) {
		wlr_log(L_DEBUG, "Set %s transform to %d", oc->name, oc->transform);
		wlr_output_set_transform(wlr_output, oc->transform);
	}

	// Find position for it
	if (oc && (oc->x != -1 || oc->y != -1)) {
		wlr_log(L_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		wlr_output_layout_add(root_container.sway_root->output_layout,
			wlr_output, oc->x, oc->y);
	} else {
		wlr_output_layout_add_auto(root_container.sway_root->output_layout,
			wlr_output);
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
		if (output->sway_output->bg_pid != 0) {
			terminate_swaybg(output->sway_output->bg_pid);
		}

		wlr_log(L_DEBUG, "Setting background for output %d to %s",
				output_i, oc->background);

		size_t len = snprintf(NULL, 0, "%s %d %s %s",
				config->swaybg_command ? config->swaybg_command : "swaybg",
				output_i, oc->background, oc->background_option);
		char *command = malloc(len + 1);
		if (!command) {
			wlr_log(L_DEBUG, "Unable to allocate swaybg command");
			return;
		}
		snprintf(command, len + 1, "%s %d %s %s",
				config->swaybg_command ? config->swaybg_command : "swaybg",
				output_i, oc->background, oc->background_option);
		wlr_log(L_DEBUG, "-> %s", command);

		char *const cmd[] = { "sh", "-c", command, NULL };
		output->sway_output->bg_pid = fork();
		if (output->sway_output->bg_pid == 0) {
			execvp(cmd[0], cmd);
		}
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
