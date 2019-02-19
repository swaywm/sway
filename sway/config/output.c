#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include "log.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/root.h"

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
	if (src->background_fallback) {
		free(dst->background_fallback);
		dst->background_fallback = strdup(src->background_fallback);
	}
	if (src->dpms_state != 0) {
		dst->dpms_state = src->dpms_state;
	}
}

static void merge_wildcard_on_all(struct output_config *wildcard) {
	for (int i = 0; i < config->output_configs->length; i++) {
		struct output_config *oc = config->output_configs->items[i];
		if (strcmp(wildcard->name, oc->name) != 0) {
			sway_log(SWAY_DEBUG, "Merging output * config on %s", oc->name);
			merge_output_config(oc, wildcard);
		}
	}
}

struct output_config *store_output_config(struct output_config *oc) {
	bool wildcard = strcmp(oc->name, "*") == 0;
	if (wildcard) {
		merge_wildcard_on_all(oc);
	}

	int i = list_seq_find(config->output_configs, output_name_cmp, oc->name);
	if (i >= 0) {
		sway_log(SWAY_DEBUG, "Merging on top of existing output config");
		struct output_config *current = config->output_configs->items[i];
		merge_output_config(current, oc);
		free_output_config(oc);
		oc = current;
	} else if (!wildcard) {
		sway_log(SWAY_DEBUG, "Adding non-wildcard output config");
		i = list_seq_find(config->output_configs, output_name_cmp, "*");
		if (i >= 0) {
			sway_log(SWAY_DEBUG, "Merging on top of output * config");
			struct output_config *current = new_output_config(oc->name);
			merge_output_config(current, config->output_configs->items[i]);
			merge_output_config(current, oc);
			free_output_config(oc);
			oc = current;
		}
		list_add(config->output_configs, oc);
	} else {
		// New wildcard config. Just add it
		sway_log(SWAY_DEBUG, "Adding output * config");
		list_add(config->output_configs, oc);
	}

	sway_log(SWAY_DEBUG, "Config stored for output %s (enabled: %d) (%dx%d@%fHz "
		"position %d,%d scale %f transform %d) (bg %s %s) (dpms %d)",
		oc->name, oc->enabled, oc->width, oc->height, oc->refresh_rate,
		oc->x, oc->y, oc->scale, oc->transform, oc->background,
		oc->background_option, oc->dpms_state);

	return oc;
}

static bool set_mode(struct wlr_output *output, int width, int height,
		float refresh_rate) {
	int mhz = (int)(refresh_rate * 1000);
	if (wl_list_empty(&output->modes)) {
		sway_log(SWAY_DEBUG, "Assigning custom mode to %s", output->name);
		return wlr_output_set_custom_mode(output, width, height, mhz);
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
		sway_log(SWAY_ERROR, "Configured mode for %s not available", output->name);
		sway_log(SWAY_INFO, "Picking default mode instead");
		best = wl_container_of(output->modes.prev, mode, link);
	} else {
		sway_log(SWAY_DEBUG, "Assigning configured mode to %s", output->name);
	}
	return wlr_output_set_mode(output, best);
}

static void handle_swaybg_client_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, swaybg_client_destroy);
	wl_list_remove(&output->swaybg_client_destroy.link);
	wl_list_init(&output->swaybg_client_destroy.link);
	output->swaybg_client = NULL;
}

static bool set_cloexec(int fd, bool cloexec) {
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		sway_log_errno(SWAY_ERROR, "fcntl failed");
		return false;
	}
	if (cloexec) {
		flags = flags | FD_CLOEXEC;
	} else {
		flags = flags & ~FD_CLOEXEC;
	}
	if (fcntl(fd, F_SETFD, flags) == -1) {
		sway_log_errno(SWAY_ERROR, "fcntl failed");
		return false;
	}
	return true;
}

static bool spawn_swaybg(struct sway_output *output, char *const cmd[]) {
	int sockets[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
		sway_log_errno(SWAY_ERROR, "socketpair failed");
		return false;
	}
	if (!set_cloexec(sockets[0], true) || !set_cloexec(sockets[1], true)) {
		return false;
	}

	output->swaybg_client = wl_client_create(server.wl_display, sockets[0]);
	if (output->swaybg_client == NULL) {
		sway_log_errno(SWAY_ERROR, "wl_client_create failed");
		return false;
	}

	output->swaybg_client_destroy.notify = handle_swaybg_client_destroy;
	wl_client_add_destroy_listener(output->swaybg_client,
		&output->swaybg_client_destroy);

	pid_t pid = fork();
	if (pid < 0) {
		sway_log_errno(SWAY_ERROR, "fork failed");
		return false;
	} else if (pid == 0) {
		pid = fork();
		if (pid < 0) {
			sway_log_errno(SWAY_ERROR, "fork failed");
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			if (!set_cloexec(sockets[1], false)) {
				exit(EXIT_FAILURE);
			}

			char wayland_socket_str[16];
			snprintf(wayland_socket_str, sizeof(wayland_socket_str),
				"%d", sockets[1]);
			setenv("WAYLAND_SOCKET", wayland_socket_str, true);

			execvp(cmd[0], cmd);
			sway_log_errno(SWAY_ERROR, "execvp failed");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}

	if (close(sockets[1]) != 0) {
		sway_log_errno(SWAY_ERROR, "close failed");
		return false;
	}
	if (waitpid(pid, NULL, 0) < 0) {
		sway_log_errno(SWAY_ERROR, "waitpid failed");
		return false;
	}

	return true;
}

bool apply_output_config(struct output_config *oc, struct sway_output *output) {
	if (output == root->noop_output) {
		return false;
	}

	struct wlr_output *wlr_output = output->wlr_output;

	if (oc && !oc->enabled) {
		// Output is configured to be disabled
		if (output->enabled) {
			output_disable(output);
			wlr_output_layout_remove(root->output_layout, wlr_output);
		}
		wlr_output_enable(wlr_output, false);
		return true;
	} else if (!output->enabled) {
		// Output is not enabled. Enable it, output_enable will call us again.
		if (!oc || oc->dpms_state != DPMS_OFF) {
			wlr_output_enable(wlr_output, true);
		}
		output_enable(output, oc);
		return true;
	}

	if (oc && oc->dpms_state == DPMS_ON) {
		sway_log(SWAY_DEBUG, "Turning on screen");
		wlr_output_enable(wlr_output, true);
	}

	bool modeset_success;
	if (oc && oc->width > 0 && oc->height > 0) {
		sway_log(SWAY_DEBUG, "Set %s mode to %dx%d (%f GHz)", oc->name, oc->width,
			oc->height, oc->refresh_rate);
		modeset_success =
			set_mode(wlr_output, oc->width, oc->height, oc->refresh_rate);
	} else if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		modeset_success = wlr_output_set_mode(wlr_output, mode);
	} else {
		// Output doesn't support modes
		modeset_success = true;
	}
	if (!modeset_success) {
		// Failed to modeset, maybe the output is missing a CRTC. Leave the
		// output disabled for now and try again when the output gets the mode
		// we asked for.
		sway_log(SWAY_ERROR, "Failed to modeset output %s", wlr_output->name);
		return false;
	}

	if (oc && oc->scale > 0) {
		sway_log(SWAY_DEBUG, "Set %s scale to %f", oc->name, oc->scale);
		wlr_output_set_scale(wlr_output, oc->scale);
	}
	if (oc && oc->transform >= 0) {
		sway_log(SWAY_DEBUG, "Set %s transform to %d", oc->name, oc->transform);
		wlr_output_set_transform(wlr_output, oc->transform);
	}

	// Find position for it
	if (oc && (oc->x != -1 || oc->y != -1)) {
		sway_log(SWAY_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		wlr_output_layout_add(root->output_layout, wlr_output, oc->x, oc->y);
	} else {
		wlr_output_layout_add_auto(root->output_layout, wlr_output);
	}

	if (output->swaybg_client != NULL) {
		wl_client_destroy(output->swaybg_client);
	}
	if (oc && oc->background && config->swaybg_command) {
		sway_log(SWAY_DEBUG, "Setting background for output %s to %s",
			wlr_output->name, oc->background);

		char *const cmd[] = {
			config->swaybg_command,
			wlr_output->name,
			oc->background,
			oc->background_option,
			oc->background_fallback ? oc->background_fallback : NULL,
			NULL,
		};
		if (!spawn_swaybg(output, cmd)) {
			return false;
		}
	}

	if (oc && oc->dpms_state == DPMS_OFF) {
		sway_log(SWAY_DEBUG, "Turning off screen");
		wlr_output_enable(wlr_output, false);
	}

	return true;
}

static void default_output_config(struct output_config *oc,
		struct wlr_output *wlr_output) {
	oc->enabled = 1;
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		oc->width = mode->width;
		oc->height = mode->height;
		oc->refresh_rate = mode->refresh;
	}
	oc->x = oc->y = -1;
	oc->scale = 1;
	oc->transform = WL_OUTPUT_TRANSFORM_NORMAL;
	oc->dpms_state = DPMS_ON;
}

static struct output_config *get_output_config(char *identifier,
		struct sway_output *sway_output) {
	const char *name = sway_output->wlr_output->name;
	struct output_config *oc_name = NULL;
	int i = list_seq_find(config->output_configs, output_name_cmp, name);
	if (i >= 0) {
		oc_name = config->output_configs->items[i];
	}

	struct output_config *oc_id = NULL;
	i = list_seq_find(config->output_configs, output_name_cmp, identifier);
	if (i >= 0) {
		oc_id = config->output_configs->items[i];
	}

	struct output_config *result = result = new_output_config("temp");
	if (config->reloading) {
		default_output_config(result, sway_output->wlr_output);
	}
	if (oc_name && oc_id) {
		// Generate a config named `<identifier> on <name>` which contains a
		// merged copy of the identifier on name. This will make sure that both
		// identifier and name configs are respected, with identifier getting
		// priority
		size_t length = snprintf(NULL, 0, "%s on %s", identifier, name) + 1;
		char *temp = malloc(length);
		snprintf(temp, length, "%s on %s", identifier, name);

		free(result->name);
		result->name = temp;
		merge_output_config(result, oc_name);
		merge_output_config(result, oc_id);

		sway_log(SWAY_DEBUG, "Generated output config \"%s\" (enabled: %d)"
			" (%dx%d@%fHz position %d,%d scale %f transform %d) (bg %s %s)"
			" (dpms %d)", result->name, result->enabled, result->width,
			result->height, result->refresh_rate, result->x, result->y,
			result->scale, result->transform, result->background,
			result->background_option, result->dpms_state);
	} else if (oc_name) {
		// No identifier config, just return a copy of the name config
		free(result->name);
		result->name = strdup(name);
		merge_output_config(result, oc_name);
	} else if (oc_id) {
		// No name config, just return a copy of the identifier config
		free(result->name);
		result->name = strdup(identifier);
		merge_output_config(result, oc_id);
	} else if (config->reloading) {
		// Neither config exists, but we need to reset the output so create a
		// default config for the output and if a wildcard config exists, merge
		// that on top
		free(result->name);
		result->name = strdup("*");
		i = list_seq_find(config->output_configs, output_name_cmp, "*");
		if (i >= 0) {
			merge_output_config(result, config->output_configs->items[i]);
		}
	} else {
		free_output_config(result);
		result = NULL;
	}

	return result;
}

void apply_output_config_to_outputs(struct output_config *oc) {
	// Try to find the output container and apply configuration now. If
	// this is during startup then there will be no container and config
	// will be applied during normal "new output" event from wlroots.
	bool wildcard = strcmp(oc->name, "*") == 0;
	char id[128];
	struct sway_output *sway_output;
	wl_list_for_each(sway_output, &root->all_outputs, link) {
		char *name = sway_output->wlr_output->name;
		output_get_identifier(id, sizeof(id), sway_output);
		if (wildcard || !strcmp(name, oc->name) || !strcmp(id, oc->name)) {
			struct output_config *current = new_output_config(oc->name);
			merge_output_config(current, oc);
			if (wildcard) {
				struct output_config *tmp = get_output_config(id, sway_output);
				if (tmp) {
					free_output_config(current);
					current = tmp;
				}
			}
			apply_output_config(current, sway_output);
			free_output_config(current);

			if (!wildcard) {
				// Stop looking if the output config isn't applicable to all
				// outputs
				break;
			}
		}
	}
}

void reset_outputs(void) {
	struct output_config *oc = NULL;
	int i = list_seq_find(config->output_configs, output_name_cmp, "*");
	if (i >= 0) {
		oc = config->output_configs->items[i];
	} else {
		oc = store_output_config(new_output_config("*"));
	}
	apply_output_config_to_outputs(oc);
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
