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
#include <wordexp.h>
#include <wlr/util/log.h>
#include "swaylock/seat.h"
#include "swaylock/swaylock.h"
#include "background-image.h"
#include "pool-buffer.h"
#include "cairo.h"
#include "log.h"
#include "stringop.h"
#include "util.h"
#include "wlr-input-inhibitor-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

void sway_terminate(int exit_code) {
	exit(exit_code);
}

static void daemonize() {
	int fds[2];
	if (pipe(fds) != 0) {
		wlr_log(WLR_ERROR, "Failed to pipe");
		exit(1);
	}
	if (fork() == 0) {
		setsid();
		close(fds[0]);
		int devnull = open("/dev/null", O_RDWR);
		dup2(STDOUT_FILENO, devnull);
		dup2(STDERR_FILENO, devnull);
		close(devnull);
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
			wlr_log(WLR_ERROR, "Failed to daemonize");
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

static const struct zwlr_layer_surface_v1_listener layer_surface_listener;

static cairo_surface_t *select_image(struct swaylock_state *state,
		struct swaylock_surface *surface);

static bool surface_is_opaque(struct swaylock_surface *surface) {
	if (surface->image) {
		return cairo_surface_get_content(surface->image) == CAIRO_CONTENT_COLOR;
	}
	return (surface->state->args.colors.background & 0xff) == 0xff;
}

static void create_layer_surface(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;

	surface->image = select_image(state, surface);

	surface->surface = wl_compositor_create_surface(state->compositor);
	assert(surface->surface);

	surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			state->layer_shell, surface->surface, surface->output,
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

	if (surface_is_opaque(surface) &&
			surface->state->args.mode != BACKGROUND_MODE_CENTER &&
			surface->state->args.mode != BACKGROUND_MODE_FIT) {
		struct wl_region *region =
			wl_compositor_create_region(surface->state->compositor);
		wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);
		wl_surface_set_opaque_region(surface->surface, region);
		wl_region_destroy(region);
	}

	wl_surface_commit(surface->surface);
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

static const struct wl_callback_listener surface_frame_listener;

static void surface_frame_handle_done(void *data, struct wl_callback *callback,
		uint32_t time) {
	struct swaylock_surface *surface = data;

	wl_callback_destroy(callback);
	surface->frame_pending = false;

	if (surface->dirty) {
		// Schedule a frame in case the surface is damaged again
		struct wl_callback *callback = wl_surface_frame(surface->surface);
		wl_callback_add_listener(callback, &surface_frame_listener, surface);
		surface->frame_pending = true;

		render_frame(surface);
		surface->dirty = false;
	}
}

static const struct wl_callback_listener surface_frame_listener = {
	.done = surface_frame_handle_done,
};

void damage_surface(struct swaylock_surface *surface) {
	surface->dirty = true;
	if (surface->frame_pending) {
		return;
	}

	struct wl_callback *callback = wl_surface_frame(surface->surface);
	wl_callback_add_listener(callback, &surface_frame_listener, surface);
	surface->frame_pending = true;
	wl_surface_commit(surface->surface);
}

void damage_state(struct swaylock_state *state) {
	struct swaylock_surface *surface;
	wl_list_for_each(surface, &state->surfaces, link) {
		damage_surface(surface);
	}
}

static void handle_wl_output_geometry(void *data, struct wl_output *output,
		int32_t x, int32_t y, int32_t width_mm, int32_t height_mm,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	// Who cares
}

static void handle_wl_output_mode(void *data, struct wl_output *output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	// Who cares
}

static void handle_wl_output_done(void *data, struct wl_output *output) {
	// Who cares
}

static void handle_wl_output_scale(void *data, struct wl_output *output,
		int32_t factor) {
	struct swaylock_surface *surface = data;
	surface->scale = factor;
	if (surface->state->run_display) {
		damage_surface(surface);
	}
}

struct wl_output_listener _wl_output_listener = {
	.geometry = handle_wl_output_geometry,
	.mode = handle_wl_output_mode,
	.done = handle_wl_output_done,
	.scale = handle_wl_output_scale,
};

static void handle_xdg_output_logical_size(void *data, struct zxdg_output_v1 *output,
		int width, int height) {
	// Who cares
}

static void handle_xdg_output_logical_position(void *data,
		struct zxdg_output_v1 *output, int x, int y) {
	// Who cares
}

static void handle_xdg_output_name(void *data, struct zxdg_output_v1 *output,
		const char *name) {
	wlr_log(WLR_DEBUG, "output name is %s", name);
	struct swaylock_surface *surface = data;
	surface->xdg_output = output;
	surface->output_name = strdup(name);
}

static void handle_xdg_output_description(void *data, struct zxdg_output_v1 *output,
		const char *description) {
	// Who cares
}

static void handle_xdg_output_done(void *data, struct zxdg_output_v1 *output) {
	// Who cares
}

struct zxdg_output_v1_listener _xdg_output_listener = {
	.logical_position = handle_xdg_output_logical_position,
	.logical_size = handle_xdg_output_logical_size,
	.done = handle_xdg_output_done,
	.name = handle_xdg_output_name,
	.description = handle_xdg_output_description,
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
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
		state->zxdg_output_manager = wl_registry_bind(
				registry, name, &zxdg_output_manager_v1_interface, 2);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct swaylock_surface *surface =
			calloc(1, sizeof(struct swaylock_surface));
		surface->state = state;
		surface->output = wl_registry_bind(registry, name,
				&wl_output_interface, 3);
		surface->output_global_name = name;
		wl_output_add_listener(surface->output, &_wl_output_listener, surface);
		wl_list_insert(&state->surfaces, &surface->link);

		if (state->run_display) {
			create_layer_surface(surface);
			wl_display_roundtrip(state->display);
		}
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

static cairo_surface_t *select_image(struct swaylock_state *state,
		struct swaylock_surface *surface) {
	struct swaylock_image *image;
	cairo_surface_t *default_image = NULL;
	wl_list_for_each(image, &state->images, link) {
		if (lenient_strcmp(image->output_name, surface->output_name) == 0) {
			return image->cairo_surface;
		} else if (!image->output_name) {
			default_image = image->cairo_surface;
		}
	}
	return default_image;
}

static void load_image(char *arg, struct swaylock_state *state) {
	// [<output>:]<path>
	struct swaylock_image *image = calloc(1, sizeof(struct swaylock_image));
	char *separator = strchr(arg, ':');
	if (separator) {
		*separator = '\0';
		image->output_name = strdup(arg);
		image->path = strdup(separator + 1);
	} else {
		image->output_name = NULL;
		image->path = strdup(arg);
	}

	bool exists = false;
	struct swaylock_image *iter_image;
	wl_list_for_each(iter_image, &state->images, link) {
		if (lenient_strcmp(iter_image->output_name, image->output_name) == 0) {
			exists = true;
			break;
		}
	}
	if (exists) {
		if (image->output_name) {
			wlr_log(WLR_ERROR, "Multiple images defined for output %s",
				image->output_name);
		} else {
			wlr_log(WLR_ERROR, "Multiple default images defined");
		}
	}

	// Bash doesn't replace the ~ with $HOME if the output name is supplied
	wordexp_t p;
	if (wordexp(image->path, &p, 0) == 0) {
		free(image->path);
		image->path = strdup(p.we_wordv[0]);
		wordfree(&p);
	}

	// Load the actual image
	image->cairo_surface = load_background_image(image->path);
	if (!image->cairo_surface) {
		free(image);
		return;
	}
	wl_list_insert(&state->images, &image->link);
	state->args.mode = BACKGROUND_MODE_FILL;
	wlr_log(WLR_DEBUG, "Loaded image %s for output %s",
			image->path, image->output_name ? image->output_name : "*");
}

static void set_default_colors(struct swaylock_colors *colors) {
	colors->background = 0xFFFFFFFF;
	colors->bs_highlight = 0xDB3300FF;
	colors->key_highlight = 0x33DB00FF;
	colors->separator = 0x000000FF;
	colors->inside = (struct swaylock_colorset){
		.input = 0x000000C0,
		.cleared = 0xE5A445C0,
		.verifying = 0x0072FFC0,
		.wrong = 0xFA0000C0,
	};
	colors->line = (struct swaylock_colorset){
		.input = 0x000000FF,
		.cleared = 0x000000FF,
		.verifying = 0x000000FF,
		.wrong = 0x000000FF,
	};
	colors->ring = (struct swaylock_colorset){
		.input = 0x337D00FF,
		.cleared = 0xE5A445FF,
		.verifying = 0x3300FFFF,
		.wrong = 0x7D3300FF,
	};
	colors->text = (struct swaylock_colorset){
		.input = 0xE5A445FF,
		.cleared = 0x000000FF,
		.verifying = 0x000000FF,
		.wrong = 0x000000FF,
	};
}

static struct swaylock_state state;

int main(int argc, char **argv) {
	enum line_mode {
		LM_LINE,
		LM_INSIDE,
		LM_RING,
	};

	enum long_option_codes {
		LO_BS_HL_COLOR = 256,
		LO_FONT,
		LO_IND_RADIUS,
		LO_IND_THICKNESS,
		LO_INSIDE_COLOR,
		LO_INSIDE_CLEAR_COLOR,
		LO_INSIDE_VER_COLOR,
		LO_INSIDE_WRONG_COLOR,
		LO_KEY_HL_COLOR,
		LO_LINE_COLOR,
		LO_LINE_CLEAR_COLOR,
		LO_LINE_VER_COLOR,
		LO_LINE_WRONG_COLOR,
		LO_RING_COLOR,
		LO_RING_CLEAR_COLOR,
		LO_RING_VER_COLOR,
		LO_RING_WRONG_COLOR,
		LO_SEP_COLOR,
		LO_TEXT_COLOR,
		LO_TEXT_CLEAR_COLOR,
		LO_TEXT_VER_COLOR,
		LO_TEXT_WRONG_COLOR,
	};

	static struct option long_options[] = {
		{"color", required_argument, NULL, 'c'},
		{"ignore-empty-password", no_argument, NULL, 'e'},
		{"daemonize", no_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{"image", required_argument, NULL, 'i'},
		{"line-uses-inside", no_argument, NULL, 'n'},
		{"socket", required_argument, NULL, 'p'},
		{"line-uses-ring", no_argument, NULL, 'r'},
		{"scaling", required_argument, NULL, 's'},
		{"tiling", no_argument, NULL, 't'},
		{"no-unlock-indicator", no_argument, NULL, 'u'},
		{"version", no_argument, NULL, 'v'},
		{"bs-hl-color", required_argument, NULL, LO_BS_HL_COLOR},
		{"font", required_argument, NULL, LO_FONT},
		{"indicator-radius", required_argument, NULL, LO_IND_RADIUS},
		{"indicator-thickness", required_argument, NULL, LO_IND_THICKNESS},
		{"inside-color", required_argument, NULL, LO_INSIDE_COLOR},
		{"inside-clear-color", required_argument, NULL, LO_INSIDE_CLEAR_COLOR},
		{"inside-ver-color", required_argument, NULL, LO_INSIDE_VER_COLOR},
		{"inside-wrong-color", required_argument, NULL, LO_INSIDE_WRONG_COLOR},
		{"key-hl-color", required_argument, NULL, LO_KEY_HL_COLOR},
		{"line-color", required_argument, NULL, LO_LINE_COLOR},
		{"line-clear-color", required_argument, NULL, LO_LINE_CLEAR_COLOR},
		{"line-ver-color", required_argument, NULL, LO_LINE_VER_COLOR},
		{"line-wrong-color", required_argument, NULL, LO_LINE_WRONG_COLOR},
		{"ring-color", required_argument, NULL, LO_RING_COLOR},
		{"ring-clear-color", required_argument, NULL, LO_RING_CLEAR_COLOR},
		{"ring-ver-color", required_argument, NULL, LO_RING_VER_COLOR},
		{"ring-wrong-color", required_argument, NULL, LO_RING_WRONG_COLOR},
		{"separator-color", required_argument, NULL, LO_SEP_COLOR},
		{"text-color", required_argument, NULL, LO_TEXT_COLOR},
		{"text-clear-color", required_argument, NULL, LO_TEXT_CLEAR_COLOR},
		{"text-ver-color", required_argument, NULL, LO_TEXT_VER_COLOR},
		{"text-wrong-color", required_argument, NULL, LO_TEXT_WRONG_COLOR},
		{0, 0, 0, 0}
	};

	const char usage[] =
		"Usage: swaylock [options...]\n"
		"\n"
		"  -c, --color <color>            "
			"Turn the screen into the given color instead of white.\n"
		"  -e, --ignore-empty-password    "
			"When an empty password is provided, do not validate it.\n"
		"  -f, --daemonize                "
			"Detach from the controlling terminal after locking.\n"
		"  -h, --help                     "
			"Show help message and quit.\n"
		"  -i, --image [<output>:]<path>  "
			"Display the given image.\n"
		"  -s, --scaling <mode>           "
			"Scaling mode: stretch, fill, fit, center, tile.\n"
		"  -t, --tiling                   "
			"Same as --scaling=tile.\n"
		"  -u, --no-unlock-indicator      "
			"Disable the unlock indicator.\n"
		"  -v, --version                  "
			"Show the version number and quit.\n"
		"  --bs-hl-color <color>          "
			"Sets the color of backspace highlight segments.\n"
		"  --font <font>                  "
			"Sets the font of the text.\n"
		"  --indicator-radius <radius>    "
			"Sets the indicator radius.\n"
		"  --indicator-thickness <thick>  "
			"Sets the indicator thickness.\n"
		"  --inside-color <color>         "
			"Sets the color of the inside of the indicator.\n"
		"  --inside-clear-color <color>   "
			"Sets the color of the inside of the indicator when cleared.\n"
		"  --inside-ver-color <color>     "
			"Sets the color of the inside of the indicator when verifying.\n"
		"  --inside-wrong-color <color>   "
			"Sets the color of the inside of the indicator when invalid.\n"
		"  --key-hl-color <color>         "
			"Sets the color of the key press highlight segments.\n"
		"  --line-color <color>           "
			"Sets the color of the line between the inside and ring.\n"
		"  --line-clear-color <color>     "
			"Sets the color of the line between the inside and ring when "
			"cleared.\n"
		"  --line-ver-color <color>       "
			"Sets the color of the line between the inside and ring when "
			"verifying.\n"
		"  --line-wrong-color <color>     "
			"Sets the color of the line between the inside and ring when "
			"invalid.\n"
		"  -n, --line-uses-inside         "
			"Use the inside color for the line between the inside and ring.\n"
		"  -r, --line-uses-ring           "
			"Use the ring color for the line between the inside and ring.\n"
		"  --ring-color <color>           "
			"Sets the color of the ring of the indicator.\n"
		"  --ring-clear-color <color>     "
			"Sets the color of the ring of the indicator when cleared.\n"
		"  --ring-ver-color <color>       "
			"Sets the color of the ring of the indicator when verifying.\n"
		"  --ring-wrong-color <color>     "
			"Sets the color of the ring of the indicator when invalid.\n"
		"  --separator-color <color>      "
			"Sets the color of the lines that separate highlight segments.\n"
		"  --text-color <color>           "
			"Sets the color of the text.\n"
		"  --text-clear-color <color>     "
			"Sets the color of the text when cleared.\n"
		"  --text-ver-color <color>       "
			"Sets the color of the text when verifying.\n"
		"  --text-wrong-color <color>     "
			"Sets the color of the text when invalid.\n"
		"\n"
		"All <color> options are of the form <rrggbb[aa]>.\n";

	enum line_mode line_mode = LM_LINE;
	state.args = (struct swaylock_args){
		.mode = BACKGROUND_MODE_SOLID_COLOR,
		.font = strdup("sans-serif"),
		.radius = 50,
		.thickness = 10,
		.ignore_empty = false,
		.show_indicator = true,
	};
	wl_list_init(&state.images);
	set_default_colors(&state.args.colors);

	wlr_log_init(WLR_DEBUG, NULL);

	int c;
	while (1) {
		int opt_idx = 0;
		c = getopt_long(argc, argv, "c:efhi:nrs:tuv", long_options, &opt_idx);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'c':
			state.args.colors.background = parse_color(optarg);
			state.args.mode = BACKGROUND_MODE_SOLID_COLOR;
			break;
		case 'e':
			state.args.ignore_empty = true;
			break;
		case 'f':
			state.args.daemonize = true;
			break;
		case 'i':
			load_image(optarg, &state);
			break;
		case 'n':
			line_mode = LM_INSIDE;
			break;
		case 'r':
			line_mode = LM_RING;
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
		case 'u':
			state.args.show_indicator = false;
			break;
		case 'v':
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "swaylock version %s (%s, branch \"%s\")\n",
					SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version unknown\n");
#endif
			return 0;
		case LO_BS_HL_COLOR:
			state.args.colors.bs_highlight = parse_color(optarg);
			break;
		case LO_FONT:
			free(state.args.font);
			state.args.font = strdup(optarg);
			break;
		case LO_IND_RADIUS:
			state.args.radius = strtol(optarg, NULL, 0);
			break;
		case LO_IND_THICKNESS:
			state.args.thickness = strtol(optarg, NULL, 0);
			break;
		case LO_INSIDE_COLOR:
			state.args.colors.inside.input = parse_color(optarg);
			break;
		case LO_INSIDE_CLEAR_COLOR:
			state.args.colors.inside.cleared = parse_color(optarg);
			break;
		case LO_INSIDE_VER_COLOR:
			state.args.colors.inside.verifying = parse_color(optarg);
			break;
		case LO_INSIDE_WRONG_COLOR:
			state.args.colors.inside.wrong = parse_color(optarg);
			break;
		case LO_KEY_HL_COLOR:
			state.args.colors.key_highlight = parse_color(optarg);
			break;
		case LO_LINE_COLOR:
			state.args.colors.line.input = parse_color(optarg);
			break;
		case LO_LINE_CLEAR_COLOR:
			state.args.colors.line.cleared = parse_color(optarg);
			break;
		case LO_LINE_VER_COLOR:
			state.args.colors.line.verifying = parse_color(optarg);
			break;
		case LO_LINE_WRONG_COLOR:
			state.args.colors.line.wrong = parse_color(optarg);
			break;
		case LO_RING_COLOR:
			state.args.colors.ring.input = parse_color(optarg);
			break;
		case LO_RING_CLEAR_COLOR:
			state.args.colors.ring.cleared = parse_color(optarg);
			break;
		case LO_RING_VER_COLOR:
			state.args.colors.ring.verifying = parse_color(optarg);
			break;
		case LO_RING_WRONG_COLOR:
			state.args.colors.ring.wrong = parse_color(optarg);
			break;
		case LO_SEP_COLOR:
			state.args.colors.separator = parse_color(optarg);
			break;
		case LO_TEXT_COLOR:
			state.args.colors.text.input = parse_color(optarg);
			break;
		case LO_TEXT_CLEAR_COLOR:
			state.args.colors.text.cleared = parse_color(optarg);
			break;
		case LO_TEXT_VER_COLOR:
			state.args.colors.text.verifying = parse_color(optarg);
			break;
		case LO_TEXT_WRONG_COLOR:
			state.args.colors.text.wrong = parse_color(optarg);
			break;
		default:
			fprintf(stderr, "%s", usage);
			return 1;
		}
	}

	if (line_mode == LM_INSIDE) {
		state.args.colors.line = state.args.colors.inside;
	} else if (line_mode == LM_RING) {
		state.args.colors.line = state.args.colors.ring;
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
		wlr_log(WLR_ERROR, "Compositor does not support the input inhibitor "
				"protocol, refusing to run insecurely");
		return 1;
	}

	if (wl_list_empty(&state.surfaces)) {
		wlr_log(WLR_DEBUG, "Exiting - no outputs to show on.");
		return 0;
	}

	zwlr_input_inhibit_manager_v1_get_inhibitor(state.input_inhibit_manager);

	if (state.zxdg_output_manager) {
		struct swaylock_surface *surface;
		wl_list_for_each(surface, &state.surfaces, link) {
			surface->xdg_output = zxdg_output_manager_v1_get_xdg_output(
						state.zxdg_output_manager, surface->output);
			zxdg_output_v1_add_listener(
					surface->xdg_output, &_xdg_output_listener, surface);
		}
		wl_display_roundtrip(state.display);
	} else {
		wlr_log(WLR_INFO, "Compositor does not support zxdg output manager, "
				"images assigned to named outputs will not work");
	}

	struct swaylock_surface *surface;
	wl_list_for_each(surface, &state.surfaces, link) {
		create_layer_surface(surface);
	}

	if (state.args.daemonize) {
		wl_display_roundtrip(state.display);
		daemonize();
	}

	state.run_display = true;
	while (wl_display_dispatch(state.display) != -1 && state.run_display) {
		// This space intentionally left blank
	}

	free(state.args.font);
	return 0;
}
