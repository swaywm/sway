#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wlr/util/log.h>
#include "swaylock/seat.h"
#include "swaylock/swaylock.h"
#include "background-image.h"
#include "pool-buffer.h"
#include "cairo.h"
#include "log.h"
#include "util.h"
#include "wlr-input-inhibitor-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

void sway_terminate(int exit_code) {
	exit(exit_code);
}

static void daemonize() {
	int fds[2];
	if (pipe(fds) != 0) {
		wlr_log(L_ERROR, "Failed to pipe");
		exit(1);
	}
	if (fork() == 0) {
		close(fds[0]);
		int devnull = open("/dev/null", O_RDWR);
		dup2(STDOUT_FILENO, devnull);
		dup2(STDERR_FILENO, devnull);
		uint8_t success = 0;
		if (chdir("/") != 0) {
			write(fds[1], &success, 1);
			exit(1);
		}
		success = 1;
		if (write(fds[1], &success, 1) != 1) {
			exit(1);
		}
		close(fds[1]);
	} else {
		close(fds[1]);
		uint8_t success;
		if (read(fds[0], &success, 1) != 1 || !success) {
			wlr_log(L_ERROR, "Failed to daemonize");
			exit(1);
		}
		close(fds[0]);
		exit(0);
	}
}

static void destroy_surface(struct swaylock_surface *surface) {
	wl_list_remove(&surface->link);
	if (surface->layer_surface != NULL) {
		zwlr_layer_surface_v1_destroy(surface->layer_surface);
	}
	if (surface->surface != NULL) {
		wl_surface_destroy(surface->surface);
	}
	destroy_buffer(&surface->buffers[0]);
	destroy_buffer(&surface->buffers[1]);
	wl_output_destroy(surface->output);
	free(surface);
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *layer_surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct swaylock_surface *surface = data;
	surface->width = width;
	surface->height = height;
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	render_frame(surface);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *layer_surface) {
	struct swaylock_surface *surface = data;
	destroy_surface(surface);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void output_geometry(void *data, struct wl_output *output, int32_t x,
		int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel,
		const char *make, const char *model, int32_t transform) {
	// Who cares
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh) {
	// Who cares
}

static void output_done(void *data, struct wl_output *output) {
	// Who cares
}

static void output_scale(void *data, struct wl_output *output, int32_t factor) {
	struct swaylock_surface *surface = data;
	surface->scale = factor;
	if (surface->state->run_display) {
		render_frames(surface->state);
	}
}

struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct swaylock_state *state = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 3);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name,
				&wl_shm_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		struct wl_seat *seat = wl_registry_bind(
				registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(seat, &seat_listener, state);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(
				registry, name, &zwlr_layer_shell_v1_interface, 1);
	} else if (strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name) == 0) {
		state->input_inhibit_manager = wl_registry_bind(
				registry, name, &zwlr_input_inhibit_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct swaylock_surface *surface =
			calloc(1, sizeof(struct swaylock_surface));
		surface->state = state;
		surface->output = wl_registry_bind(registry, name,
				&wl_output_interface, 3);
		surface->output_global_name = name;
		wl_output_add_listener(surface->output, &output_listener, surface);
		wl_list_insert(&state->surfaces, &surface->link);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	struct swaylock_state *state = data;
	struct swaylock_surface *surface;
	wl_list_for_each(surface, &state->surfaces, link) {
		if (surface->output_global_name == name) {
			destroy_surface(surface);
			break;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static struct swaylock_state state;

int main(int argc, char **argv) {
	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"color", required_argument, NULL, 'c'},
		{"image", required_argument, NULL, 'i'},
		{"scaling", required_argument, NULL, 's'},
		{"tiling", no_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 'p'},
		{"no-unlock-indicator", no_argument, NULL, 'u'},
		{"daemonize", no_argument, NULL, 'f'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaylock [options...]\n"
		"\n"
		"  -h, --help                     Show help message and quit.\n"
		"  -c, --color <rrggbb[aa]>       Turn the screen into the given color instead of white.\n"
		"  -s, --scaling                  Scaling mode: stretch, fill, fit, center, tile.\n"
		"  -t, --tiling                   Same as --scaling=tile.\n"
		"  -v, --version                  Show the version number and quit.\n"
		"  -i, --image [<output>:]<path>  Display the given image.\n"
		"  -u, --no-unlock-indicator      Disable the unlock indicator.\n"
		"  -f, --daemonize                Detach from the controlling terminal.\n"
		"  --socket <socket>              Use the specified socket.\n";

	struct swaylock_args args = {
		.mode = BACKGROUND_MODE_SOLID_COLOR,
		.color = 0xFFFFFFFF,
		.show_indicator = true,
	};
	cairo_surface_t *background_image = NULL;
	state.args = args;
	wlr_log_init(L_DEBUG, NULL);

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hc:i:s:tvuf", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'c': {
			state.args.color = parse_color(optarg);
			state.args.mode = BACKGROUND_MODE_SOLID_COLOR;
			break;
		}
		case 'i':
			// TODO: Multiple background images (bleh)
			background_image = load_background_image(optarg);
			if (!background_image) {
				return 1;
			}
			state.args.mode = BACKGROUND_MODE_FILL;
			break;
		case 's':
			state.args.mode = parse_background_mode(optarg);
			if (state.args.mode == BACKGROUND_MODE_INVALID) {
				return 1;
			}
			break;
		case 't':
			state.args.mode = BACKGROUND_MODE_TILE;
			break;
		case 'v':
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "swaylock version %s (%s, branch \"%s\")\n",
					SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version unknown\n");
#endif
			return 0;
		case 'u':
			state.args.show_indicator = false;
			break;
		case 'f':
			daemonize();
			break;
		default:
			fprintf(stderr, "%s", usage);
			return 1;
		}
	}

#ifdef __linux__
	// Most non-linux platforms require root to mlock()
	if (mlock(state.password.buffer, sizeof(state.password.buffer)) != 0) {
		sway_abort("Unable to mlock() password memory.");
	}
#endif

	wl_list_init(&state.surfaces);
	state.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	state.display = wl_display_connect(NULL);
	assert(state.display);

	struct wl_registry *registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);
	assert(state.compositor && state.layer_shell && state.shm);
	if (!state.input_inhibit_manager) {
		wlr_log(L_ERROR, "Compositor does not support the input inhibitor "
				"protocol, refusing to run insecurely");
	}

	if (wl_list_empty(&state.surfaces)) {
		wlr_log(L_DEBUG, "Exiting - no outputs to show on.");
		return 0;
	}

	struct swaylock_surface *surface;
	wl_list_for_each(surface, &state.surfaces, link) {
		surface->image = background_image;

		surface->surface = wl_compositor_create_surface(state.compositor);
		assert(surface->surface);

		surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
				state.layer_shell, surface->surface, surface->output,
				ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "lockscreen");
		assert(surface->layer_surface);

		zwlr_layer_surface_v1_set_size(surface->layer_surface, 0, 0);
		zwlr_layer_surface_v1_set_anchor(surface->layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
		zwlr_layer_surface_v1_set_exclusive_zone(surface->layer_surface, -1);
		zwlr_layer_surface_v1_set_keyboard_interactivity(
				surface->layer_surface, true);
		zwlr_layer_surface_v1_add_listener(surface->layer_surface,
				&layer_surface_listener, surface);
		wl_surface_commit(surface->surface);
		wl_display_roundtrip(state.display);
	}

	zwlr_input_inhibit_manager_v1_get_inhibitor(state.input_inhibit_manager);

	state.run_display = true;
	while (wl_display_dispatch(state.display) != -1 && state.run_display) {
		// This space intentionally left blank
	}
	return 0;
}
