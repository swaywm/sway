#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fnmatch.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/output.h"
#include "sway/tree/root.h"
#include "log.h"
#include "util.h"

void output_get_identifier(char *identifier, size_t len,
		struct sway_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;
	snprintf(identifier, len, "%s %s %s", wlr_output->make, wlr_output->model,
		wlr_output->serial);
}

const char *sway_output_scale_filter_to_string(enum scale_filter_mode scale_filter) {
	switch (scale_filter) {
	case SCALE_FILTER_DEFAULT:
		return "smart";
	case SCALE_FILTER_LINEAR:
		return "linear";
	case SCALE_FILTER_NEAREST:
		return "nearest";
	case SCALE_FILTER_SMART:
		return "smart";
	}
	sway_assert(false, "Unknown value for scale_filter.");
	return NULL;
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
	oc->custom_mode = -1;
	oc->x = oc->y = -1;
	oc->scale = -1;
	oc->scale_filter = SCALE_FILTER_DEFAULT;
	oc->transform = -1;
	oc->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	oc->max_render_time = -1;
	oc->adaptive_sync = -1;
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
	if (src->scale_filter != SCALE_FILTER_DEFAULT) {
		dst->scale_filter = src->scale_filter;
	}
	if (src->subpixel != WL_OUTPUT_SUBPIXEL_UNKNOWN) {
		dst->subpixel = src->subpixel;
	}
	if (src->refresh_rate != -1) {
		dst->refresh_rate = src->refresh_rate;
	}
	if (src->custom_mode != -1) {
		dst->custom_mode = src->custom_mode;
	}
	if (src->transform != -1) {
		dst->transform = src->transform;
	}
	if (src->max_render_time != -1) {
		dst->max_render_time = src->max_render_time;
	}
	if (src->adaptive_sync != -1) {
		dst->adaptive_sync = src->adaptive_sync;
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

enum output_config_changes {
	OUTPUT_CONFIG_ENABLED = 1 << 1,
	OUTPUT_CONFIG_WIDTH = 1 << 2,
	OUTPUT_CONFIG_HEIGHT = 1 << 3,
	OUTPUT_CONFIG_X = 1 << 4,
	OUTPUT_CONFIG_Y = 1 << 5,
	OUTPUT_CONFIG_SCALE = 1 << 6,
	OUTPUT_CONFIG_SCALE_FILTER = 1 << 7,
	OUTPUT_CONFIG_SUBPIXEL = 1 << 8,
	OUTPUT_CONFIG_REFRESH_RATE = 1 << 9,
	OUTPUT_CONFIG_CUSTOM_MODE = 1 << 10,
	OUTPUT_CONFIG_TRANSFORM = 1 << 11,
	OUTPUT_CONFIG_MAX_RENDER_TIME = 1 << 12,
	OUTPUT_CONFIG_ADAPTIVE_SYNC = 1 << 13,
	OUTPUT_CONFIG_BACKGROUND = 1 << 14,
	OUTPUT_CONFIG_BACKGROUND_OPTION = 1 << 15,
	OUTPUT_CONFIG_BACKGROUND_FALLBACK = 1 << 16,
	OUTPUT_CONFIG_DPMS_STATE = 1 << 17,
};

static int output_config_changes(struct output_config *src) {
	int changes = 0;

	if (src->enabled != -1) {
		changes |= OUTPUT_CONFIG_ENABLED;
	}
	if (src->width != -1) {
		changes |= OUTPUT_CONFIG_WIDTH;
	}
	if (src->height != -1) {
		changes |= OUTPUT_CONFIG_HEIGHT;
	}
	if (src->x != -1) {
		changes |= OUTPUT_CONFIG_X;
	}
	if (src->y != -1) {
		changes |= OUTPUT_CONFIG_Y;
	}
	if (src->scale != -1) {
		changes |= OUTPUT_CONFIG_SCALE;
	}
	if (src->scale_filter != SCALE_FILTER_DEFAULT) {
		changes |= OUTPUT_CONFIG_SCALE_FILTER;
	}
	if (src->subpixel != WL_OUTPUT_SUBPIXEL_UNKNOWN) {
		changes |= OUTPUT_CONFIG_SUBPIXEL;
	}
	if (src->refresh_rate != -1) {
		changes |= OUTPUT_CONFIG_REFRESH_RATE;
	}
	if (src->custom_mode != -1) {
		changes |= OUTPUT_CONFIG_CUSTOM_MODE;
	}
	if (src->transform != -1) {
		changes |= OUTPUT_CONFIG_TRANSFORM;
	}
	if (src->max_render_time != -1) {
		changes |= OUTPUT_CONFIG_MAX_RENDER_TIME;
	}
	if (src->adaptive_sync != -1) {
		changes |= OUTPUT_CONFIG_ADAPTIVE_SYNC;
	}
	if (src->background) {
		changes |= OUTPUT_CONFIG_BACKGROUND;
	}
	if (src->background_option) {
		changes |= OUTPUT_CONFIG_BACKGROUND_OPTION;
	}
	if (src->background_fallback) {
		changes |= OUTPUT_CONFIG_BACKGROUND_FALLBACK;
	}
	if (src->dpms_state != 0) {
		changes |= OUTPUT_CONFIG_DPMS_STATE;
	}

	return changes;
}

struct output_config *store_output_config(struct output_config *oc) {
	// Get rid of a previous output config that has the same name and changes
	// the same things, as it has been superseded. Since we do this on every
	// insert there can only ever be one of these. This prevents the output
	// config list of commands growing unbounded.
	for (int i = 0; i < config->output_configs->length; ++i) {
		struct output_config *list_oc = config->output_configs->items[i];
		if (!strcmp(oc->name, list_oc->name) &&
			output_config_changes(oc) == output_config_changes(list_oc)) {
			free_output_config(config->output_configs->items[i]);
			list_del(config->output_configs, i);
			break;
		}
	}

	// Then simply add the command to the bottom of the list where it will be
	// applied last and thus have the biggest priority.
	list_add(config->output_configs, oc);

	sway_log(SWAY_DEBUG, "Config stored for output %s (enabled: %d) (%dx%d@%fHz "
		"position %d,%d scale %f subpixel %s transform %d) (bg %s %s) (dpms %d) "
		"(max render time: %d)",
		oc->name, oc->enabled, oc->width, oc->height, oc->refresh_rate,
		oc->x, oc->y, oc->scale, sway_wl_output_subpixel_to_string(oc->subpixel),
		oc->transform, oc->background, oc->background_option, oc->dpms_state,
		oc->max_render_time);

	return oc;
}

static void set_mode(struct wlr_output *output, int width, int height,
		float refresh_rate, bool custom) {
	int mhz = (int)(refresh_rate * 1000);

	if (wl_list_empty(&output->modes) || custom) {
		sway_log(SWAY_DEBUG, "Assigning custom mode to %s", output->name);
		wlr_output_set_custom_mode(output, width, height,
			refresh_rate > 0 ? mhz : 0);
		return;
	}

	struct wlr_output_mode *mode, *best = NULL;
	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width == width && mode->height == height) {
			if (mode->refresh == mhz) {
				best = mode;
				break;
			}
			if (best == NULL || mode->refresh > best->refresh) {
				best = mode;
			}
		}
	}
	if (!best) {
		sway_log(SWAY_ERROR, "Configured mode for %s not available", output->name);
		sway_log(SWAY_INFO, "Picking preferred mode instead");
		best = wlr_output_preferred_mode(output);
	} else {
		sway_log(SWAY_DEBUG, "Assigning configured mode to %s", output->name);
	}
	wlr_output_set_mode(output, best);
}

/* Some manufacturers hardcode the aspect-ratio of the output in the physical
 * size field. */
static bool phys_size_is_aspect_ratio(struct wlr_output *output) {
	return (output->phys_width == 1600 && output->phys_height == 900) ||
		(output->phys_width == 1600 && output->phys_height == 1000) ||
		(output->phys_width == 160 && output->phys_height == 90) ||
		(output->phys_width == 160 && output->phys_height == 100) ||
		(output->phys_width == 16 && output->phys_height == 9) ||
		(output->phys_width == 16 && output->phys_height == 10);
}

// The minimum DPI at which we turn on a scale of 2
#define HIDPI_DPI_LIMIT (2 * 96)
// The minimum screen height at which we turn on a scale of 2
#define HIDPI_MIN_HEIGHT 1200
// 1 inch = 25.4 mm
#define MM_PER_INCH 25.4

static int compute_default_scale(struct wlr_output *output) {
	struct wlr_box box = { .width = output->width, .height = output->height };
	if (output->pending.committed & WLR_OUTPUT_STATE_MODE) {
		switch (output->pending.mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			box.width = output->pending.mode->width;
			box.height = output->pending.mode->height;
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			box.width = output->pending.custom_mode.width;
			box.height = output->pending.custom_mode.height;
			break;
		}
	}
	enum wl_output_transform transform = output->transform;
	if (output->pending.committed & WLR_OUTPUT_STATE_TRANSFORM) {
		transform = output->pending.transform;
	}
	wlr_box_transform(&box, &box, transform, box.width, box.height);

	int width = box.width;
	int height = box.height;

	if (height < HIDPI_MIN_HEIGHT) {
		return 1;
	}

	if (output->phys_width == 0 || output->phys_height == 0) {
		return 1;
	}

	if (phys_size_is_aspect_ratio(output)) {
		return 1;
	}

	double dpi_x = (double) width / (output->phys_width / MM_PER_INCH);
	double dpi_y = (double) height / (output->phys_height / MM_PER_INCH);
	sway_log(SWAY_DEBUG, "Output DPI: %fx%f", dpi_x, dpi_y);
	if (dpi_x <= HIDPI_DPI_LIMIT || dpi_y <= HIDPI_DPI_LIMIT) {
		return 1;
	}

	return 2;
}

static void queue_output_config(struct output_config *oc,
		struct sway_output *output) {
	if (output == root->noop_output) {
		return;
	}

	struct wlr_output *wlr_output = output->wlr_output;

	if (oc && (!oc->enabled || oc->dpms_state == DPMS_OFF)) {
		sway_log(SWAY_DEBUG, "Turning off output %s", wlr_output->name);
		wlr_output_enable(wlr_output, false);
		return;
	}

	sway_log(SWAY_DEBUG, "Turning on output %s", wlr_output->name);
	wlr_output_enable(wlr_output, true);

	if (oc && oc->width > 0 && oc->height > 0) {
		sway_log(SWAY_DEBUG, "Set %s mode to %dx%d (%f Hz)",
			wlr_output->name, oc->width, oc->height, oc->refresh_rate);
		set_mode(wlr_output, oc->width, oc->height,
			oc->refresh_rate, oc->custom_mode == 1);
	} else if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
	}

	if (oc && (oc->subpixel != WL_OUTPUT_SUBPIXEL_UNKNOWN || config->reloading)) {
		sway_log(SWAY_DEBUG, "Set %s subpixel to %s", oc->name,
			sway_wl_output_subpixel_to_string(oc->subpixel));
		wlr_output_set_subpixel(wlr_output, oc->subpixel);
	}

	if (oc && oc->transform >= 0) {
		sway_log(SWAY_DEBUG, "Set %s transform to %d", oc->name, oc->transform);
		wlr_output_set_transform(wlr_output, oc->transform);
	}

	// Apply the scale last before the commit, because the scale auto-detection
	// reads the pending output size
	float scale;
	if (oc && oc->scale > 0) {
		scale = oc->scale;
	} else {
		scale = compute_default_scale(wlr_output);
		sway_log(SWAY_DEBUG, "Auto-detected output scale: %f", scale);
	}
	if (scale != wlr_output->scale) {
		sway_log(SWAY_DEBUG, "Set %s scale to %f", wlr_output->name, scale);
		wlr_output_set_scale(wlr_output, scale);
	}

	if (oc && oc->adaptive_sync != -1) {
		sway_log(SWAY_DEBUG, "Set %s adaptive sync to %d", wlr_output->name,
			oc->adaptive_sync);
		wlr_output_enable_adaptive_sync(wlr_output, oc->adaptive_sync == 1);
	}
}

bool apply_output_config(struct output_config *oc, struct sway_output *output) {
	if (output == root->noop_output) {
		return false;
	}

	struct wlr_output *wlr_output = output->wlr_output;

	// Flag to prevent the output mode event handler from calling us
	output->enabling = (!oc || oc->enabled);

	queue_output_config(oc, output);

	if (!oc || oc->dpms_state != DPMS_OFF) {
		output->current_mode = wlr_output->pending.mode;
	}

	sway_log(SWAY_DEBUG, "Committing output %s", wlr_output->name);
	if (!wlr_output_commit(wlr_output)) {
		// Failed to commit output changes, maybe the output is missing a CRTC.
		// Leave the output disabled for now and try again when the output gets
		// the mode we asked for.
		sway_log(SWAY_ERROR, "Failed to commit output %s", wlr_output->name);
		output->enabling = false;
		return false;
	}

	output->enabling = false;

	if (oc && !oc->enabled) {
		sway_log(SWAY_DEBUG, "Disabling output %s", oc->name);
		if (output->enabled) {
			output_disable(output);
			wlr_output_layout_remove(root->output_layout, wlr_output);
		}
		return true;
	}

	if (config->reloading) {
		output_damage_whole(output);
	}

	if (oc) {
		enum scale_filter_mode scale_filter_old = output->scale_filter;
		switch (oc->scale_filter) {
			case SCALE_FILTER_DEFAULT:
			case SCALE_FILTER_SMART:
				output->scale_filter = ceilf(wlr_output->scale) == wlr_output->scale ?
					SCALE_FILTER_NEAREST : SCALE_FILTER_LINEAR;
				break;
			case SCALE_FILTER_LINEAR:
			case SCALE_FILTER_NEAREST:
				output->scale_filter = oc->scale_filter;
				break;
		}
		if (scale_filter_old != output->scale_filter) {
			sway_log(SWAY_DEBUG, "Set %s scale_filter to %s", oc->name,
				sway_output_scale_filter_to_string(output->scale_filter));
		}
	}

	// Find position for it
	if (oc && (oc->x != -1 || oc->y != -1)) {
		sway_log(SWAY_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		wlr_output_layout_add(root->output_layout, wlr_output, oc->x, oc->y);
	} else {
		wlr_output_layout_add_auto(root->output_layout, wlr_output);
	}

	// Update output->{lx, ly, width, height}
	struct wlr_box *output_box =
		wlr_output_layout_get_box(root->output_layout, wlr_output);
	output->lx = output_box->x;
	output->ly = output_box->y;
	output->width = output_box->width;
	output->height = output_box->height;

	if (!output->enabled) {
		output_enable(output);
	}

	if (oc && oc->max_render_time >= 0) {
		sway_log(SWAY_DEBUG, "Set %s max render time to %d",
			oc->name, oc->max_render_time);
		output->max_render_time = oc->max_render_time;
	}

	// Reconfigure all devices, since input config may have been applied before
	// this output came online, and some config items (like map_to_output) are
	// dependent on an output being present.
	input_manager_configure_all_inputs();
	return true;
}

bool test_output_config(struct output_config *oc, struct sway_output *output) {
	if (output == root->noop_output) {
		return false;
	}

	queue_output_config(oc, output);
	bool ok = wlr_output_test(output->wlr_output);
	wlr_output_rollback(output->wlr_output);
	return ok;
}

static void default_output_config(struct output_config *oc,
		struct wlr_output *wlr_output) {
	oc->enabled = 1;
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		oc->width = mode->width;
		oc->height = mode->height;
		oc->refresh_rate = mode->refresh / 1000.f;
	}
	oc->x = oc->y = -1;
	oc->scale = 0; // auto
	oc->scale_filter = SCALE_FILTER_DEFAULT;
	struct sway_output *output = wlr_output->data;
	oc->subpixel = output->detected_subpixel;
	oc->transform = WL_OUTPUT_TRANSFORM_NORMAL;
	oc->dpms_state = DPMS_ON;
	oc->max_render_time = 0;
}

struct output_config *find_output_config(struct sway_output *sway_output) {
	// Start with a default config for this output
	struct output_config *result = new_output_config("merge");
	default_output_config(result, sway_output->wlr_output);

	// Apply all matches in order
	char id[128];
	output_get_identifier(id, sizeof(id), sway_output);
	char *name = sway_output->wlr_output->name;
	for (int i = 0; i < config->output_configs->length; ++i) {
		struct output_config *oc = config->output_configs->items[i];
		if (!strcmp(oc->name, "*") || !strcmp(oc->name, name) || !strcmp(oc->name, id)) {
			merge_output_config(result, oc);
		}
	}

	struct output_config *oc = result;
	sway_log(SWAY_DEBUG, "Found output config %s (enabled: %d) (%dx%d@%fHz "
		"position %d,%d scale %f subpixel %s transform %d) (bg %s %s) (dpms %d) "
		"(max render time: %d)",
		oc->name, oc->enabled, oc->width, oc->height, oc->refresh_rate,
		oc->x, oc->y, oc->scale, sway_wl_output_subpixel_to_string(oc->subpixel),
		oc->transform, oc->background, oc->background_option, oc->dpms_state,
		oc->max_render_time);

	return result;
}

void apply_output_config_to_outputs(void) {
	// Try to find the output container and apply configuration now. If
	// this is during startup then there will be no container and config
	// will be applied during normal "new output" event from wlroots.
	struct sway_output *sway_output, *tmp;
	wl_list_for_each_safe(sway_output, tmp, &root->all_outputs, link) {
		struct output_config *oc = find_output_config(sway_output);
		apply_output_config(oc, sway_output);
		free_output_config(oc);
	}

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
		cursor_rebase(seat->cursor);
	}
}

void reset_outputs(void) {
	apply_output_config_to_outputs();
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

static void handle_swaybg_client_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_config *sway_config =
		wl_container_of(listener, sway_config, swaybg_client_destroy);
	wl_list_remove(&sway_config->swaybg_client_destroy.link);
	wl_list_init(&sway_config->swaybg_client_destroy.link);
	sway_config->swaybg_client = NULL;
}

static bool _spawn_swaybg(char **command) {
	if (config->swaybg_client != NULL) {
		wl_client_destroy(config->swaybg_client);
	}
	int sockets[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
		sway_log_errno(SWAY_ERROR, "socketpair failed");
		return false;
	}
	if (!sway_set_cloexec(sockets[0], true) || !sway_set_cloexec(sockets[1], true)) {
		return false;
	}

	config->swaybg_client = wl_client_create(server.wl_display, sockets[0]);
	if (config->swaybg_client == NULL) {
		sway_log_errno(SWAY_ERROR, "wl_client_create failed");
		return false;
	}

	config->swaybg_client_destroy.notify = handle_swaybg_client_destroy;
	wl_client_add_destroy_listener(config->swaybg_client,
		&config->swaybg_client_destroy);

	pid_t pid = fork();
	if (pid < 0) {
		sway_log_errno(SWAY_ERROR, "fork failed");
		return false;
	} else if (pid == 0) {
		pid = fork();
		if (pid < 0) {
			sway_log_errno(SWAY_ERROR, "fork failed");
			_exit(EXIT_FAILURE);
		} else if (pid == 0) {
			if (!sway_set_cloexec(sockets[1], false)) {
				_exit(EXIT_FAILURE);
			}

			char wayland_socket_str[16];
			snprintf(wayland_socket_str, sizeof(wayland_socket_str),
				"%d", sockets[1]);
			setenv("WAYLAND_SOCKET", wayland_socket_str, true);

			execvp(command[0], command);
			sway_log_errno(SWAY_ERROR, "execvp failed");
			_exit(EXIT_FAILURE);
		}
		_exit(EXIT_SUCCESS);
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

bool spawn_swaybg(void) {
	if (!config->swaybg_command) {
		return true;
	}

	size_t length = 2;
	for (int i = 0; i < config->output_configs->length; i++) {
		struct output_config *oc = config->output_configs->items[i];
		if (!oc->background) {
			continue;
		}
		if (strcmp(oc->background_option, "solid_color") == 0) {
			length += 4;
		} else if (oc->background_fallback) {
			length += 8;
		} else {
			length += 6;
		}
	}

	char **cmd = calloc(length, sizeof(char *));
	if (!cmd) {
		sway_log(SWAY_ERROR, "Failed to allocate spawn_swaybg command");
		return false;
	}

	size_t i = 0;
	cmd[i++] = config->swaybg_command;
	for (int j = 0; j < config->output_configs->length; j++) {
		struct output_config *oc = config->output_configs->items[j];
		if (!oc->background) {
			continue;
		}
		if (strcmp(oc->background_option, "solid_color") == 0) {
			cmd[i++] = "-o";
			cmd[i++] = oc->name;
			cmd[i++] = "-c";
			cmd[i++] = oc->background;
		} else {
			cmd[i++] = "-o";
			cmd[i++] = oc->name;
			cmd[i++] = "-i";
			cmd[i++] = oc->background;
			cmd[i++] = "-m";
			cmd[i++] = oc->background_option;
			if (oc->background_fallback) {
				cmd[i++] = "-c";
				cmd[i++] = oc->background_fallback;
			}
		}
		assert(i <= length);
	}

	for (size_t k = 0; k < i; k++) {
		sway_log(SWAY_DEBUG, "spawn_swaybg cmd[%zd] = %s", k, cmd[k]);
	}

	bool result = _spawn_swaybg(cmd);
	free(cmd);
	return result;
}
