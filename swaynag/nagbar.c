#define _XOPEN_SOURCE 500
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "log.h"
#include "list.h"
#include "swaynag/nagbar.h"
#include "swaynag/render.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static void nop() {
	// Intentionally left blank
}

static bool terminal_execute(char *terminal, char *command) {
	char fname[] = "/tmp/swaynagXXXXXX";
	FILE *tmp= fdopen(mkstemp(fname), "w");
	if (!tmp) {
		wlr_log(WLR_ERROR, "Failed to create temp script");
		return false;
	}
	wlr_log(WLR_DEBUG, "Created temp script: %s", fname);
	fprintf(tmp, "#!/bin/sh\nrm %s\n%s", fname, command);
	fclose(tmp);
	chmod(fname, S_IRUSR | S_IWUSR | S_IXUSR);
	char cmd[strlen(terminal) + strlen(" -e ") + strlen(fname) + 1];
	sprintf(cmd, "%s -e %s", terminal, fname);
	execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
	return true;
}

static void nagbar_button_execute(struct sway_nagbar *nagbar,
		struct sway_nagbar_button *button) {
	wlr_log(WLR_DEBUG, "Executing [%s]: %s", button->text, button->action);
	if (button->type == NAGBAR_ACTION_DISMISS) {
		nagbar->run_display = false;
	} else if (button->type == NAGBAR_ACTION_EXPAND) {
		nagbar->details.visible = !nagbar->details.visible;
		render_frame(nagbar);
	} else {
		if (fork() == 0) {
			// Child process. Will be used to prevent zombie processes
			setsid();
			if (fork() == 0) {
				// Child of the child. Will be reparented to the init process
				char *terminal = getenv("TERMINAL");
				if (terminal && strlen(terminal)) {
					wlr_log(WLR_DEBUG, "Found $TERMINAL: %s", terminal);
					if (!terminal_execute(terminal, button->action)) {
						nagbar_destroy(nagbar);
						exit(EXIT_FAILURE);
					}
				} else {
					wlr_log(WLR_DEBUG, "$TERMINAL not found. Running directly");
					execl("/bin/sh", "/bin/sh", "-c", button->action, NULL);
				}
			}
			exit(EXIT_SUCCESS);
		}
	}
	wait(0);
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct sway_nagbar *nagbar = data;
	nagbar->width = width;
	nagbar->height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	render_frame(nagbar);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct sway_nagbar *nagbar = data;
	nagbar_destroy(nagbar);
}

static struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct sway_nagbar *nagbar = data;
	struct sway_nagbar_pointer *pointer = &nagbar->pointer;
	wl_surface_set_buffer_scale(pointer->cursor_surface, nagbar->scale);
	wl_surface_attach(pointer->cursor_surface,
			wl_cursor_image_get_buffer(pointer->cursor_image), 0, 0);
	wl_pointer_set_cursor(wl_pointer, serial, pointer->cursor_surface,
			pointer->cursor_image->hotspot_x / nagbar->scale,
			pointer->cursor_image->hotspot_y / nagbar->scale);
	wl_surface_commit(pointer->cursor_surface);
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct sway_nagbar *nagbar = data;
	nagbar->pointer.x = wl_fixed_to_int(surface_x);
	nagbar->pointer.y = wl_fixed_to_int(surface_y);
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	struct sway_nagbar *nagbar = data;

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}

	double x = nagbar->pointer.x * nagbar->scale;
	double y = nagbar->pointer.y * nagbar->scale;
	for (int i = 0; i < nagbar->buttons->length; i++) {
		struct sway_nagbar_button *nagbutton = nagbar->buttons->items[i];
		if (x >= nagbutton->x
				&& y >= nagbutton->y
				&& x < nagbutton->x + nagbutton->width
				&& y < nagbutton->y + nagbutton->height) {
			nagbar_button_execute(nagbar, nagbutton);
			return;
		}
	}

	if (nagbar->details.visible &&
			nagbar->details.total_lines != nagbar->details.visible_lines) {
		struct sway_nagbar_button button_up = nagbar->details.button_up;
		if (x >= button_up.x
				&& y >= button_up.y
				&& x < button_up.x + button_up.width
				&& y < button_up.y + button_up.height
				&& nagbar->details.offset > 0) {
			nagbar->details.offset--;
			render_frame(nagbar);
			return;
		}

		struct sway_nagbar_button button_down = nagbar->details.button_down;
		int bot = nagbar->details.total_lines - nagbar->details.visible_lines;
		if (x >= button_down.x
				&& y >= button_down.y
				&& x < button_down.x + button_down.width
				&& y < button_down.y + button_down.height
				&& nagbar->details.offset < bot) {
			nagbar->details.offset++;
			render_frame(nagbar);
			return;
		}
	}
}

static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value) {
	struct sway_nagbar *nagbar = data;
	if (!nagbar->details.visible
			|| nagbar->pointer.x < nagbar->details.x
			|| nagbar->pointer.y < nagbar->details.y
			|| nagbar->pointer.x >= nagbar->details.x + nagbar->details.width
			|| nagbar->pointer.y >= nagbar->details.y + nagbar->details.height
			|| nagbar->details.total_lines == nagbar->details.visible_lines) {
		return;
	}

	int direction = wl_fixed_to_int(value);
	int bot = nagbar->details.total_lines - nagbar->details.visible_lines;
	if (direction < 0 && nagbar->details.offset > 0) {
		nagbar->details.offset--;
	} else if (direction > 0 && nagbar->details.offset < bot) {
		nagbar->details.offset++;
	}

	render_frame(nagbar);
}

static struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = nop,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = nop,
	.axis_source = nop,
	.axis_stop = nop,
	.axis_discrete = nop,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps) {
	struct sway_nagbar *nagbar = data;
	if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
		nagbar->pointer.pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(nagbar->pointer.pointer, &pointer_listener,
				nagbar);
	}
}

const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = nop,
};

static void output_scale(void *data, struct wl_output *output,
		int32_t factor) {
	struct sway_nagbar *nagbar = data;
	nagbar->scale = factor;
	render_frame(nagbar);
}

static struct wl_output_listener output_listener = {
	.geometry = nop,
	.mode = nop,
	.done = nop,
	.scale = output_scale,
};

struct output_state {
	struct wl_output *wl_output;
	uint32_t wl_name;
	struct zxdg_output_v1 *xdg_output;
	struct sway_nagbar *nagbar;
};

static void xdg_output_handle_name(void *data,
		struct zxdg_output_v1 *xdg_output, const char *name) {
	struct output_state *state = data;
	char *outname = state->nagbar->output.name;
	wlr_log(WLR_DEBUG, "Checking against output %s for %s", name, outname);
	if ((!outname && !state->nagbar->output.wl_output)
			|| (name && outname && strcmp(name, outname) == 0)) {
		wlr_log(WLR_DEBUG, "Using output %s", name);
		state->nagbar->output.wl_output = state->wl_output;
		state->nagbar->output.wl_name = state->wl_name;
		wl_output_add_listener(state->nagbar->output.wl_output,
				&output_listener, state->nagbar);
		wl_display_roundtrip(state->nagbar->display);
		zxdg_output_v1_destroy(state->xdg_output);
	} else {
		zxdg_output_v1_destroy(state->xdg_output);
		wl_output_destroy(state->wl_output);
	}
	state->nagbar->querying_outputs--;
	free(state);
}

static struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = nop,
	.logical_size = nop,
	.done = nop,
	.name = xdg_output_handle_name,
	.description = nop,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct sway_nagbar *nagbar = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		nagbar->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 3);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		nagbar->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(nagbar->seat, &seat_listener, nagbar);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		nagbar->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		if (!nagbar->output.wl_output && nagbar->xdg_output_manager) {
			nagbar->querying_outputs++;
			struct output_state *state =
				calloc(1, sizeof(struct output_state));
			state->nagbar = nagbar;
			state->wl_output = wl_registry_bind(registry, name,
					&wl_output_interface, 3);
			state->wl_name = name;
			state->xdg_output = zxdg_output_manager_v1_get_xdg_output(
					nagbar->xdg_output_manager, state->wl_output);
			zxdg_output_v1_add_listener(state->xdg_output,
					&xdg_output_listener, state);
		} else if (!nagbar->output.wl_output && !nagbar->xdg_output_manager) {
			wlr_log(WLR_ERROR, "Warning: zxdg_output_manager_v1 not supported."
					" Falling back to first detected output");
			nagbar->output.wl_output = wl_registry_bind(registry, name,
					&wl_output_interface, 3);
			nagbar->output.wl_name = name;
			wl_output_add_listener(nagbar->output.wl_output,
					&output_listener, nagbar);
		}
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		nagbar->layer_shell = wl_registry_bind(
				registry, name, &zwlr_layer_shell_v1_interface, 1);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0
			&& version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
		nagbar->xdg_output_manager = wl_registry_bind(registry, name,
				&zxdg_output_manager_v1_interface,
				ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	struct sway_nagbar *nagbar = data;
	if (nagbar->output.wl_name == name) {
		nagbar->run_display = false;
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

void nagbar_setup(struct sway_nagbar *nagbar) {
	nagbar->display = wl_display_connect(NULL);
	assert(nagbar->display);

	nagbar->scale = 1;

	struct wl_registry *registry = wl_display_get_registry(nagbar->display);
	wl_registry_add_listener(registry, &registry_listener, nagbar);
	wl_display_roundtrip(nagbar->display);
	assert(nagbar->compositor && nagbar->layer_shell && nagbar->shm);

	while (nagbar->querying_outputs > 0) {
		wl_display_roundtrip(nagbar->display);
	}

	if (!nagbar->output.wl_output) {
		if (nagbar->output.name) {
			wlr_log(WLR_ERROR, "Output '%s' not found", nagbar->output.name);
		} else {
			wlr_log(WLR_ERROR, "No outputs detected");
		}
		nagbar_destroy(nagbar);
		exit(EXIT_FAILURE);
	}

	struct sway_nagbar_pointer *pointer = &nagbar->pointer;
	int scale = nagbar->scale < 1 ? 1 : nagbar->scale;
	pointer->cursor_theme = wl_cursor_theme_load(
			NULL, 24 * scale, nagbar->shm);
	assert(pointer->cursor_theme);
	struct wl_cursor *cursor =
		wl_cursor_theme_get_cursor(pointer->cursor_theme, "left_ptr");
	assert(cursor);
	pointer->cursor_image = cursor->images[0];
	pointer->cursor_surface = wl_compositor_create_surface(nagbar->compositor);
	assert(pointer->cursor_surface);

	nagbar->surface = wl_compositor_create_surface(nagbar->compositor);
	assert(nagbar->surface);
	nagbar->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			nagbar->layer_shell, nagbar->surface, nagbar->output.wl_output,
			ZWLR_LAYER_SHELL_V1_LAYER_TOP, "swaynag");
	assert(nagbar->layer_surface);
	zwlr_layer_surface_v1_add_listener(nagbar->layer_surface,
			&layer_surface_listener, nagbar);
	zwlr_layer_surface_v1_set_anchor(nagbar->layer_surface, nagbar->anchors);

	wl_registry_destroy(registry);
}

void nagbar_run(struct sway_nagbar *nagbar) {
	nagbar->run_display = true;
	render_frame(nagbar);
	while (nagbar->run_display && wl_display_dispatch(nagbar->display) != -1) {
		// This is intentionally left blank
	}
}

void nagbar_destroy(struct sway_nagbar *nagbar) {
	nagbar->run_display = false;

	free(nagbar->message);
	free(nagbar->font);
	while (nagbar->buttons->length) {
		struct sway_nagbar_button *button = nagbar->buttons->items[0];
		list_del(nagbar->buttons, 0);
		free(button->text);
		free(button->action);
		free(button);
	}
	list_free(nagbar->buttons);
	free(nagbar->details.message);
	free(nagbar->details.button_up.text);
	free(nagbar->details.button_down.text);

	if (nagbar->layer_surface) {
		zwlr_layer_surface_v1_destroy(nagbar->layer_surface);
	}

	if (nagbar->surface) {
		wl_surface_destroy(nagbar->surface);
	}

	if (nagbar->output.wl_output) {
		wl_output_destroy(nagbar->output.wl_output);
	}

	if (&nagbar->buffers[0]) {
		destroy_buffer(&nagbar->buffers[0]);
	}

	if (&nagbar->buffers[1]) {
		destroy_buffer(&nagbar->buffers[1]);
	}

	if (nagbar->compositor) {
		wl_compositor_destroy(nagbar->compositor);
	}

	if (nagbar->shm) {
		wl_shm_destroy(nagbar->shm);
	}

	if (nagbar->display) {
		wl_display_disconnect(nagbar->display);
	}
}
