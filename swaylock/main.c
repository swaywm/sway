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
#include <xkbcommon/xkbcommon.h>
#include "background-image.h"
#include "pool-buffer.h"
#include "cairo.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct swaylock_args {
	uint32_t color;
	enum background_mode mode;
	bool show_indicator;
};

enum mod_bit {
	MOD_SHIFT = 1<<0,
	MOD_CAPS = 1<<1,
	MOD_CTRL = 1<<2,
	MOD_ALT = 1<<3,
	MOD_MOD2 = 1<<4,
	MOD_MOD3 = 1<<5,
	MOD_LOGO = 1<<6,
	MOD_MOD5 = 1<<7,
};

enum mask {
	MASK_SHIFT,
	MASK_CAPS,
	MASK_CTRL,
	MASK_ALT,
	MASK_MOD2,
	MASK_MOD3,
	MASK_LOGO,
	MASK_MOD5,
	MASK_LAST
};

const char *XKB_MASK_NAMES[MASK_LAST] = {
	XKB_MOD_NAME_SHIFT,
	XKB_MOD_NAME_CAPS,
	XKB_MOD_NAME_CTRL,
	XKB_MOD_NAME_ALT,
	"Mod2",
	"Mod3",
	XKB_MOD_NAME_LOGO,
	"Mod5",
};

const enum mod_bit XKB_MODS[MASK_LAST] = {
	MOD_SHIFT,
	MOD_CAPS,
	MOD_CTRL,
	MOD_ALT,
	MOD_MOD2,
	MOD_MOD3,
	MOD_LOGO,
	MOD_MOD5
};

struct swaylock_xkb {
	uint32_t modifiers;
	struct xkb_state *state;
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	xkb_mod_mask_t masks[MASK_LAST];
};

struct swaylock_state {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;
	struct wl_list contexts;
	struct swaylock_args args;
	struct swaylock_xkb xkb;
	bool run_display;
};

struct swaylock_context {
	cairo_surface_t *image;
	struct swaylock_state *state;
	struct wl_output *output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
	uint32_t width, height;
	struct wl_list link;
};

static void daemonize() {
	if (fork() == 0) {
		int devnull = open("/dev/null", O_RDWR);
		dup2(STDOUT_FILENO, devnull);
		dup2(STDERR_FILENO, devnull);
		chdir("/");
	} else {
		exit(0);
	}
}

static void render_frame(struct swaylock_context *context) {
	struct swaylock_state *state = context->state;
	context->current_buffer = get_next_buffer(state->shm,
			context->buffers, context->width, context->height);
	cairo_t *cairo = context->current_buffer->cairo;
	if (state->args.mode == BACKGROUND_MODE_SOLID_COLOR) {
		cairo_set_source_u32(cairo, state->args.color);
		cairo_paint(cairo);
	} else {
		render_background_image(cairo, context->image,
				state->args.mode, context->width, context->height);
	}
	wl_surface_attach(context->surface, context->current_buffer->buffer, 0, 0);
	wl_surface_damage(context->surface, 0, 0, context->width, context->height);
	wl_surface_commit(context->surface);
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct swaylock_context *context = data;
	context->width = width;
	context->height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	render_frame(context);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct swaylock_context *context = data;
	zwlr_layer_surface_v1_destroy(context->layer_surface);
	wl_surface_destroy(context->surface);
	context->state->run_display = false;
}

static struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size) {
	struct swaylock_state *state = data;
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		wlr_log(L_ERROR, "Unknown keymap format %d, aborting", format);
		exit(1);
	}
	char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_shm == MAP_FAILED) {
		close(fd);
		wlr_log(L_ERROR, "Unable to initialize keymap shm, aborting");
		exit(1);
	}
	struct xkb_keymap *keymap = xkb_keymap_new_from_string(
			state->xkb.context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
	munmap(map_shm, size);
	close(fd);
	assert(keymap);
	struct xkb_state *xkb_state = xkb_state_new(keymap);
	assert(xkb_state);
	xkb_keymap_unref(state->xkb.keymap);
	xkb_state_unref(state->xkb.state);
	state->xkb.keymap = keymap;
	state->xkb.state = xkb_state;
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	// Who cares
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface) {
	// Who cares
}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t _key_state) {
	struct swaylock_state *state = data;
	enum wl_keyboard_key_state key_state = _key_state;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(state->xkb.state, key + 8);
	uint32_t keycode = key_state == WL_KEYBOARD_KEY_STATE_PRESSED ?
		key + 8 : 0;
	uint32_t codepoint = xkb_state_key_get_utf32(state->xkb.state, keycode);
	wlr_log(L_DEBUG, "%c %d", codepoint, sym);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group) {
	struct swaylock_state *state = data;
	xkb_state_update_mask(state->xkb.state,
			mods_depressed, mods_latched, mods_locked, 0, 0, group);
	xkb_mod_mask_t mask = xkb_state_serialize_mods(state->xkb.state,
			XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
	state->xkb.modifiers = 0;
	for (uint32_t i = 0; i < MASK_LAST; ++i) {
		if (mask & state->xkb.masks[i]) {
			state->xkb.modifiers |= XKB_MODS[i];
		}
	}
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay) {
	// TODO
}

static struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	wl_pointer_set_cursor(wl_pointer, serial, NULL, 0, 0);
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	// Who cares
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// Who cares
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	// Who cares
}

static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value) {
	// Who cares
}

static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	// Who cares
}

static void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis_source) {
	// Who cares
}

static void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis) {
	// Who cares
}

static void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis, int32_t discrete) {
	// Who cares
}

static struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps) {
	struct swaylock_state *state = data;
	if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
		struct wl_pointer *pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
		struct wl_keyboard *keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, state);
	}
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
		const char *name) {
	// Who cares
}

const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct swaylock_state *state = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 1);
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
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct swaylock_context *context =
			calloc(1, sizeof(struct swaylock_context));
		context->state = state;
		context->output = wl_registry_bind(registry, name,
				&wl_output_interface, 1);
		wl_list_insert(&state->contexts, &context->link);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static struct swaylock_state state;

static void sigalarm_handler(int sig) {
	signal(SIGALRM, SIG_IGN);
	// TODO: Hide typing indicator
	signal(SIGALRM, sigalarm_handler);
}

int main(int argc, char **argv) {
	signal(SIGALRM, sigalarm_handler);

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
			// TODO
			return 1;
		case 's':
			state.args.mode = parse_background_mode(optarg);
			if (state.args.mode == BACKGROUND_MODE_INVALID) {
				return 1;
			}
			break;
		case 't':
			// TODO
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

	wl_list_init(&state.contexts);
	state.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	assert(state.display = wl_display_connect(NULL));

	struct wl_registry *registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);
	assert(state.compositor && state.layer_shell && state.shm);

	if (wl_list_empty(&state.contexts)) {
		wlr_log(L_DEBUG, "Exiting - no outputs to show on.");
		return 0;
	}

	struct swaylock_context *context;
	wl_list_for_each(context, &state.contexts, link) {
		assert(context->surface =
				wl_compositor_create_surface(state.compositor));

		context->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
				state.layer_shell, context->surface, context->output,
				ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "lockscreen");
		assert(context->layer_surface);

		zwlr_layer_surface_v1_set_size(context->layer_surface, 0, 0);
		zwlr_layer_surface_v1_set_anchor(context->layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
		zwlr_layer_surface_v1_set_exclusive_zone(context->layer_surface, -1);
		zwlr_layer_surface_v1_set_keyboard_interactivity(
				context->layer_surface, true);
		zwlr_layer_surface_v1_add_listener(context->layer_surface,
				&layer_surface_listener, context);
		wl_surface_commit(context->surface);
		wl_display_roundtrip(state.display);
	}

	state.run_display = true;
	while (wl_display_dispatch(state.display) != -1 && state.run_display) {
		// This space intentionally left blank
	}
	return 0;
}
