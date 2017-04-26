#define _XOPEN_SOURCE 500
#include "wayland-swaylock-client-protocol.h"
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <security/pam_appl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include "client/window.h"
#include "client/registry.h"
#include "client/cairo.h"
#include "swaylock/swaylock.h"
#include "ipc-client.h"
#include "log.h"
#include "util.h"

struct registry *registry;
struct render_data render_data;
struct lock_config *config;
bool show_indicator = true;

void wl_dispatch_events() {
	wl_display_flush(registry->display);
	if (wl_display_dispatch(registry->display) == -1) {
		sway_log(L_ERROR, "failed to run wl_display_dispatch");
		exit(1);
	}
}

void sigalarm_handler(int sig) {
	signal(SIGALRM, SIG_IGN);
	// Hide typing indicator
	render_data.auth_state = AUTH_STATE_IDLE;
	render(&render_data, config);
	wl_display_flush(registry->display);
	signal(SIGALRM, sigalarm_handler);
}

void sway_terminate(int exit_code) {
	int i;
	for (i = 0; i < render_data.surfaces->length; ++i) {
		struct window *window = render_data.surfaces->items[i];
		window_teardown(window);
	}
	list_free(render_data.surfaces);
	if (registry) {
		registry_teardown(registry);
	}
	exit(exit_code);
}

char *password;
int password_size;
enum line_source line_source = LINE_SOURCE_DEFAULT;

struct lock_config *init_config() {
	struct lock_config *config = calloc(1, sizeof(struct lock_config));

	config->font = strdup("sans-serif");
	config->colors.text = 0x000000FF;

	config->colors.line = 0x000000FF;
	config->colors.separator = 0x000000FF;

	config->colors.input_cursor = 0x33DB00FF;
	config->colors.backspace_cursor = 0xDB3300FF;

	config->colors.normal.inner_ring = 0x000000BF;
	config->colors.normal.outer_ring = 0x337D00FF;

	config->colors.validating.inner_ring = 0x0072FFBF;
	config->colors.validating.outer_ring = 0x3300FAFF;

	config->colors.invalid.inner_ring = 0xFA0000BF;
	config->colors.invalid.outer_ring = 0x7D3300FF;

	return config;
}

void free_config(struct lock_config *config) {
	free(config->font);
	free(config);
}

int function_conversation(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr) {

	const char* msg_style_names[] = {
		NULL,
		"PAM_PROMPT_ECHO_OFF",
		"PAM_PROMPT_ECHO_ON",
		"PAM_ERROR_MSG",
		"PAM_TEXT_INFO",
	};

	/* PAM expects an array of responses, one for each message */
	struct pam_response *pam_reply = calloc(num_msg, sizeof(struct pam_response));
	*resp = pam_reply;

	for(int i=0; i<num_msg; ++i) {
		sway_log(L_DEBUG, "msg[%d]: (%s) %s", i,
				msg_style_names[msg[i]->msg_style],
				msg[i]->msg);

		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			pam_reply[i].resp = password;
			break;

		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			break;
		}
	}

	return PAM_SUCCESS;
}

/**
 * Note: PAM will free() 'password' during the process
 */
bool verify_password() {
	struct passwd *passwd = getpwuid(getuid());
	char *username = passwd->pw_name;

	const struct pam_conv local_conversation = { function_conversation, NULL };
	pam_handle_t *local_auth_handle = NULL;
	int pam_err;
	if ((pam_err = pam_start("swaylock", username, &local_conversation, &local_auth_handle)) != PAM_SUCCESS) {
		sway_abort("PAM returned %d\n", pam_err);
	}
	if ((pam_err = pam_authenticate(local_auth_handle, 0)) != PAM_SUCCESS) {
		return false;
	}
	if ((pam_err = pam_end(local_auth_handle, pam_err)) != PAM_SUCCESS) {
		return false;
	}
	return true;
}

void notify_key(enum wl_keyboard_key_state state, xkb_keysym_t sym, uint32_t code, uint32_t codepoint) {
	int redraw_screen = 0;
	char *password_realloc;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		switch (sym) {
		case XKB_KEY_KP_Enter: // fallthrough
		case XKB_KEY_Return:
			render_data.auth_state = AUTH_STATE_VALIDATING;

			render(&render_data, config);
			// Make sure our render call will actually be displayed on the screen
			wl_dispatch_events();

			if (verify_password()) {
				exit(0);
			}
			render_data.auth_state = AUTH_STATE_INVALID;
			redraw_screen = 1;

			password_size = 1024;
			password = malloc(password_size);
			password[0] = '\0';
			break;
		case XKB_KEY_BackSpace:
			{
				int i = strlen(password);
				if (i > 0) {
					password[i - 1] = '\0';
					render_data.auth_state = AUTH_STATE_BACKSPACE;
					redraw_screen = 1;
				}
				break;
			}
		case XKB_KEY_Control_L: // fallthrough
		case XKB_KEY_Control_R: // fallthrough
		case XKB_KEY_Shift_L: // fallthrough
		case XKB_KEY_Shift_R: // fallthrough
		case XKB_KEY_Caps_Lock: // fallthrough
		case XKB_KEY_Shift_Lock: // fallthrough
		case XKB_KEY_Meta_L: // fallthrough
		case XKB_KEY_Meta_R: // fallthrough
		case XKB_KEY_Alt_L: // fallthrough
		case XKB_KEY_Alt_R: // fallthrough
		case XKB_KEY_Super_L: // fallthrough
		case XKB_KEY_Super_R: // fallthrough
		case XKB_KEY_Hyper_L: // fallthrough
		case XKB_KEY_Hyper_R:
			{
				// don't draw screen on modifier keys
				break;
			}
		case XKB_KEY_Escape: // fallthrough
		case XKB_KEY_u: // fallthrough
		case XKB_KEY_U:
			{
				// clear password buffer on ctrl-u (or escape for i3lock compatibility)
				if (sym == XKB_KEY_Escape || xkb_state_mod_name_is_active(registry->input->xkb.state,
						XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
					render_data.auth_state = AUTH_STATE_BACKSPACE;
					redraw_screen = 1;

					password_size = 1024;
					free(password);
					password = malloc(password_size);
					password[0] = '\0';
					break;
				}
			}
		default:
			{
				render_data.auth_state = AUTH_STATE_INPUT;
				redraw_screen = 1;
				int i = strlen(password);
				if (i + 1 == password_size) {
					password_size += 1024;
					password_realloc = realloc(password, password_size);
					// reset password if realloc fails.
					if (password_realloc == NULL) {
						password_size = 1024;
						free(password);
						password = malloc(password_size);
						password[0] = '\0';
						break;
					} else {
						password = password_realloc;
					}
				}
				password[i] = (char)codepoint;
				password[i + 1] = '\0';
				break;
			}
		}
		if (redraw_screen) {
			render(&render_data, config);
			wl_dispatch_events();
			// Hide the indicator after a couple of seconds
			alarm(5);
		}
	}
}

void render_color(struct window *window, uint32_t color) {
	cairo_set_source_u32(window->cairo, color);
	cairo_paint(window->cairo);
}

void render_image(struct window *window, cairo_surface_t *image, enum scaling_mode scaling_mode) {
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);
	int wwidth = window->width * window->scale;
	int wheight = window->height * window->scale;

	switch (scaling_mode) {
	case SCALING_MODE_STRETCH:
		cairo_scale(window->cairo,
				(double) wwidth / width,
				(double) wheight / height);
		cairo_set_source_surface(window->cairo, image, 0, 0);
		break;
	case SCALING_MODE_FILL:
	{
		double window_ratio = (double) wwidth / wheight;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double) wwidth / width;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					0,
					(double) wheight/2 / scale - height/2);
		} else {
			double scale = (double) wheight / height;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					(double) wwidth/2 / scale - width/2,
					0);
		}
		break;
	}
	case SCALING_MODE_FIT:
	{
		double window_ratio = (double) wwidth / wheight;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double) wheight / height;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					(double) wwidth/2 / scale - width/2,
					0);
		} else {
			double scale = (double) wwidth / width;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					0,
					(double) wheight/2 / scale - height/2);
		}
		break;
	}
	case SCALING_MODE_CENTER:
		cairo_set_source_surface(window->cairo, image,
				(double) wwidth/2 - width/2,
				(double) wheight/2 - height/2);
		break;
	case SCALING_MODE_TILE:
	{
		cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
		cairo_set_source(window->cairo, pattern);
		break;
	}
	}

	cairo_paint(window->cairo);
}

cairo_surface_t *load_image(char *image_path) {
	cairo_surface_t *image = NULL;

#ifdef WITH_GDK_PIXBUF
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(image_path, &err);
	if (!pixbuf) {
		sway_abort("Failed to load background image: %s", err->message);
	}
	image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
#else
	image = cairo_image_surface_create_from_png(image_path);
#endif //WITH_GDK_PIXBUF
	if (!image) {
		sway_abort("Failed to read background image.");
	}

	return image;
}

int main(int argc, char **argv) {
	const char *scaling_mode_str = "fit", *socket_path = NULL;
	int i;
	void *images = NULL;
	config = init_config();

	render_data.num_images = 0;
	render_data.color_set = 0;
	render_data.color = 0xFFFFFFFF;
	render_data.auth_state = AUTH_STATE_IDLE;

	init_log(L_INFO);
	// Install SIGALARM handler (for hiding the typing indicator)
	signal(SIGALRM, sigalarm_handler);

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"color", required_argument, NULL, 'c'},
		{"image", required_argument, NULL, 'i'},
		{"scaling", required_argument, NULL, 0},
		{"tiling", no_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 'p'},
		{"no-unlock-indicator", no_argument, NULL, 'u'},
		{"daemonize", no_argument, NULL, 'f'},
		{"font", required_argument, NULL, 0},
		{"line-uses-ring", no_argument, NULL, 'r'},
		{"line-uses-inside", no_argument, NULL, 's'},
		{"textcolor", required_argument, NULL, 0},
		{"insidevercolor", required_argument, NULL, 0},
		{"insidewrongcolor", required_argument, NULL, 0},
		{"insidecolor", required_argument, NULL, 0},
		{"ringvercolor", required_argument, NULL, 0},
		{"ringwrongcolor", required_argument, NULL, 0},
		{"ringcolor", required_argument, NULL, 0},
		{"linecolor", required_argument, NULL, 0},
		{"separatorcolor", required_argument, NULL, 0},
		{"keyhlcolor", required_argument, NULL, 0},
		{"bshlcolor", required_argument, NULL, 0},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaylock [options...]\n"
		"\n"
		"  -h, --help                     Show help message and quit.\n"
		"  -c, --color <rrggbb[aa]>       Turn the screen into the given color instead of white.\n"
		"  --scaling                      Scaling mode: stretch, fill, fit, center, tile.\n"
		"  -t, --tiling                   Same as --scaling=tile.\n"
		"  -v, --version                  Show the version number and quit.\n"
		"  -i, --image [<output>:]<path>  Display the given image.\n"
		"  -u, --no-unlock-indicator      Disable the unlock indicator.\n"
		"  -f, --daemonize                Detach from the controlling terminal.\n"
		"  --socket <socket>              Use the specified socket.\n"
		"  For more information see `man swaylock`\n";


	registry = registry_poll();

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hc:i:srtvuf", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'c':
		{
			render_data.color = parse_color(optarg);
			render_data.color_set = 1;
			break;
		}
		case 'i':
		{
			char *image_path = strchr(optarg, ':');
			if (image_path == NULL) {
				if (render_data.num_images == 0) {
					// Provided image without output
					render_data.image = load_image(optarg);
					render_data.num_images = -1;
				} else {
					sway_log(L_ERROR, "output must be defined for all --images or no --images");
					exit(EXIT_FAILURE);
				}
			} else {
				// Provided image for all outputs
				if (render_data.num_images == 0) {
					images = calloc(registry->outputs->length, sizeof(char*) * 2);
				} else if (render_data.num_images == -1) {
					sway_log(L_ERROR, "output must be defined for all --images or no --images");
					exit(EXIT_FAILURE);
				}

				image_path[0] = '\0';
				((char**) images)[render_data.num_images * 2] = optarg;
				((char**) images)[render_data.num_images++ * 2 + 1] = ++image_path;
			}
			break;
		}
		case 't':
			scaling_mode_str = "tile";
			break;
		case 'p':
			socket_path = optarg;
			break;
		case 'v':
			fprintf(stdout, "swaylock version " SWAY_VERSION "\n");
			exit(EXIT_SUCCESS);
			break;
		case 'u':
			show_indicator = false;
			break;
		case 'f': {
			pid_t t = fork();
			if (t == -1) {
				sway_log(L_ERROR, "daemon call failed");
				exit(EXIT_FAILURE);
			} else if (t > 0) {
				exit(0);
			}
			break;
		}
		case 'r':
			if (line_source != LINE_SOURCE_DEFAULT) {
				sway_log(L_ERROR, "line source options conflict");
				exit(EXIT_FAILURE);
			}
			line_source = LINE_SOURCE_RING;
			break;
		case 's':
			if (line_source != LINE_SOURCE_DEFAULT) {
				sway_log(L_ERROR, "line source options conflict");
				exit(EXIT_FAILURE);
			}
			line_source = LINE_SOURCE_INSIDE;
			break;
		case 0:
			if (strcmp(long_options[option_index].name, "font") == 0) {
				free(config->font);
				config->font = strdup(optarg);
			} else if (strcmp(long_options[option_index].name, "scaling") == 0) {
				scaling_mode_str = optarg;
			} else if (strcmp(long_options[option_index].name, "textcolor") == 0) {
				config->colors.text = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "insidevercolor") == 0) {
				config->colors.validating.inner_ring = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "insidewrongcolor") == 0) {
				config->colors.invalid.inner_ring = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "insidecolor") == 0) {
				config->colors.normal.inner_ring = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "ringvercolor") == 0) {
				config->colors.validating.outer_ring = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "ringwrongcolor") == 0) {
				config->colors.invalid.outer_ring = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "ringcolor") == 0) {
				config->colors.normal.outer_ring = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "linecolor") == 0) {
				config->colors.line = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "separatorcolor") == 0) {
				config->colors.separator = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "keyhlcolor") == 0) {
				config->colors.input_cursor = parse_color(optarg);
			} else if (strcmp(long_options[option_index].name, "bshlcolor") == 0) {
				config->colors.backspace_cursor = parse_color(optarg);
			}
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	render_data.scaling_mode = SCALING_MODE_STRETCH;
	if (strcmp(scaling_mode_str, "stretch") == 0) {
		render_data.scaling_mode = SCALING_MODE_STRETCH;
	} else if (strcmp(scaling_mode_str, "fill") == 0) {
		render_data.scaling_mode = SCALING_MODE_FILL;
	} else if (strcmp(scaling_mode_str, "fit") == 0) {
		render_data.scaling_mode = SCALING_MODE_FIT;
	} else if (strcmp(scaling_mode_str, "center") == 0) {
		render_data.scaling_mode = SCALING_MODE_CENTER;
	} else if (strcmp(scaling_mode_str, "tile") == 0) {
		render_data.scaling_mode = SCALING_MODE_TILE;
	} else {
		sway_abort("Unsupported scaling mode: %s", scaling_mode_str);
	}

	password_size = 1024;
	password = malloc(password_size);
	password[0] = '\0';
	render_data.surfaces = create_list();
	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			sway_abort("Unable to retrieve socket path");
		}
	}

	if (!registry) {
		sway_abort("Unable to connect to wayland compositor");
	}

	if (!registry->swaylock) {
		sway_abort("swaylock requires the compositor to support the swaylock extension.");
	}

	if (registry->pointer) {
		// We don't want swaylock to have a pointer
		wl_pointer_destroy(registry->pointer);
		registry->pointer = NULL;
	}

	for (i = 0; i < registry->outputs->length; ++i) {
		struct output_state *output = registry->outputs->items[i];
		struct window *window = window_setup(registry,
				output->width, output->height, output->scale, true);
		if (!window) {
			sway_abort("Failed to create surfaces.");
		}
		list_add(render_data.surfaces, window);
	}

	registry->input->notify = notify_key;

	// Different background for the output
	if (render_data.num_images >= 1) {
		char **displays_paths = images;
		render_data.images = calloc(registry->outputs->length, sizeof(cairo_surface_t*));

		int socketfd = ipc_open_socket(socket_path);
		uint32_t len = 0;
		char *outputs = ipc_single_command(socketfd, IPC_GET_OUTPUTS, "", &len);
		struct json_object *json_outputs = json_tokener_parse(outputs);

		for (i = 0; i < registry->outputs->length; ++i) {
			if (displays_paths[i * 2] != NULL) {
				for (int j = 0;; ++j) {
					if (j >= json_object_array_length(json_outputs)) {
						sway_log(L_ERROR, "%s is not an extant output", displays_paths[i * 2]);
						exit(EXIT_FAILURE);
					}

					struct json_object *dsp_name, *at_j = json_object_array_get_idx(json_outputs, j);
					if (!json_object_object_get_ex(at_j, "name", &dsp_name)) {
						sway_abort("output doesn't have a name field");
					}
					if (!strcmp(displays_paths[i * 2], json_object_get_string(dsp_name))) {
						render_data.images[j] = load_image(displays_paths[i * 2 + 1]);
						break;
					}
				}
			}
		}

		json_object_put(json_outputs);
		close(socketfd);
		free(displays_paths);
	}

	render(&render_data, config);
	bool locked = false;
	while (wl_display_dispatch(registry->display) != -1) {
		if (!locked) {
			for (i = 0; i < registry->outputs->length; ++i) {
				struct output_state *output = registry->outputs->items[i];
				struct window *window = render_data.surfaces->items[i];
				lock_set_lock_surface(registry->swaylock, output->output, window->surface);
			}
			locked = true;
		}
	}

	// Free surfaces
	if (render_data.num_images == -1) {
		cairo_surface_destroy(render_data.image);
	} else if (render_data.num_images >= 1) {
		for (i = 0; i < registry->outputs->length; ++i) {
			if (render_data.images[i] != NULL) {
				cairo_surface_destroy(render_data.images[i]);
			}
		}
		free(render_data.images);
	}

	for (i = 0; i < render_data.surfaces->length; ++i) {
		struct window *window = render_data.surfaces->items[i];
		window_teardown(window);
	}
	list_free(render_data.surfaces);
	registry_teardown(registry);

	free_config(config);

	return 0;
}

void render(struct render_data *render_data, struct lock_config *config) {
	int i;
	for (i = 0; i < render_data->surfaces->length; ++i) {
		sway_log(L_DEBUG, "Render surface %d of %d", i, render_data->surfaces->length);
		struct window *window = render_data->surfaces->items[i];
		if (!window_prerender(window) || !window->cairo) {
			continue;
		}
		int wwidth = window->width * window->scale;
		int wheight = window->height * window->scale;

		cairo_save(window->cairo);
		cairo_set_operator(window->cairo, CAIRO_OPERATOR_CLEAR);
		cairo_paint(window->cairo);
		cairo_restore(window->cairo);

		// Reset the transformation matrix
		cairo_identity_matrix(window->cairo);

		if (render_data->num_images == 0 || render_data->color_set) {
			render_color(window, render_data->color);
		}

		if (render_data->num_images == -1) {
			// One background for all
			render_image(window, render_data->image, render_data->scaling_mode);
		} else if (render_data->num_images >= 1) {
			// Different backgrounds
			if (render_data->images[i] != NULL) {
				render_image(window, render_data->images[i], render_data->scaling_mode);
			}
		}

		// Reset the transformation matrix again
		cairo_identity_matrix(window->cairo);

		// Draw specific values (copied from i3)
		const int ARC_RADIUS = 50;
		const int ARC_THICKNESS = 10;
		const float TYPE_INDICATOR_RANGE = M_PI / 3.0f;
		const float TYPE_INDICATOR_BORDER_THICKNESS = M_PI / 128.0f;

		// Add visual indicator
		if (show_indicator && render_data->auth_state != AUTH_STATE_IDLE) {
			// Draw circle
			cairo_set_line_width(window->cairo, ARC_THICKNESS);
			cairo_arc(window->cairo, wwidth/2, wheight/2, ARC_RADIUS, 0, 2 * M_PI);
			switch (render_data->auth_state) {
			case AUTH_STATE_INPUT:
			case AUTH_STATE_BACKSPACE: {
				cairo_set_source_u32(window->cairo, config->colors.normal.inner_ring);
				cairo_fill_preserve(window->cairo);
				cairo_set_source_u32(window->cairo, config->colors.normal.outer_ring);
				cairo_stroke(window->cairo);
			} break;
			case AUTH_STATE_VALIDATING: {
				cairo_set_source_u32(window->cairo, config->colors.validating.inner_ring);
				cairo_fill_preserve(window->cairo);
				cairo_set_source_u32(window->cairo, config->colors.validating.outer_ring);
				cairo_stroke(window->cairo);
			} break;
			case AUTH_STATE_INVALID: {
				cairo_set_source_u32(window->cairo, config->colors.invalid.inner_ring);
				cairo_fill_preserve(window->cairo);
				cairo_set_source_u32(window->cairo, config->colors.invalid.outer_ring);
				cairo_stroke(window->cairo);
			} break;
			default: break;
			}

			// Draw a message
			char *text = NULL;
			cairo_set_source_u32(window->cairo, config->colors.text);
			cairo_select_font_face(window->cairo, config->font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(window->cairo, ARC_RADIUS/3.0f);
			switch (render_data->auth_state) {
			case AUTH_STATE_VALIDATING:
				text = "verifying";
				break;
			case AUTH_STATE_INVALID:
				text = "wrong";
				break;
			default: break;
			}

			if (text) {
				cairo_text_extents_t extents;
				double x, y;

				cairo_text_extents(window->cairo, text, &extents);
				x = wwidth/2 - ((extents.width/2) + extents.x_bearing);
				y = wheight/2 - ((extents.height/2) + extents.y_bearing);

				cairo_move_to(window->cairo, x, y);
				cairo_show_text(window->cairo, text);
				cairo_close_path(window->cairo);
				cairo_new_sub_path(window->cairo);
			}

			// Typing indicator: Highlight random part on keypress
			if (render_data->auth_state == AUTH_STATE_INPUT || render_data->auth_state == AUTH_STATE_BACKSPACE) {
				static double highlight_start = 0;
				highlight_start += (rand() % (int)(M_PI * 100)) / 100.0 + M_PI * 0.5;
				cairo_arc(window->cairo, wwidth/2, wheight/2, ARC_RADIUS, highlight_start, highlight_start + TYPE_INDICATOR_RANGE);
				if (render_data->auth_state == AUTH_STATE_INPUT) {
					cairo_set_source_u32(window->cairo, config->colors.input_cursor);
				} else {
					cairo_set_source_u32(window->cairo, config->colors.backspace_cursor);
				}
				cairo_stroke(window->cairo);

				// Draw borders
				cairo_set_source_u32(window->cairo, config->colors.separator);
				cairo_arc(window->cairo, wwidth/2, wheight/2, ARC_RADIUS, highlight_start, highlight_start + TYPE_INDICATOR_BORDER_THICKNESS);
				cairo_stroke(window->cairo);

				cairo_arc(window->cairo, wwidth/2, wheight/2, ARC_RADIUS, highlight_start + TYPE_INDICATOR_RANGE, (highlight_start + TYPE_INDICATOR_RANGE) + TYPE_INDICATOR_BORDER_THICKNESS);
				cairo_stroke(window->cairo);
			}

			switch(line_source) {
			case LINE_SOURCE_RING:
				switch(render_data->auth_state) {
				case AUTH_STATE_VALIDATING:
					cairo_set_source_u32(window->cairo, config->colors.validating.outer_ring);
					break;
				case AUTH_STATE_INVALID:
					cairo_set_source_u32(window->cairo, config->colors.invalid.outer_ring);
					break;
				default:
					cairo_set_source_u32(window->cairo, config->colors.normal.outer_ring);
				}
				break;
			case LINE_SOURCE_INSIDE:
				switch(render_data->auth_state) {
				case AUTH_STATE_VALIDATING:
					cairo_set_source_u32(window->cairo, config->colors.validating.inner_ring);
					break;
				case AUTH_STATE_INVALID:
					cairo_set_source_u32(window->cairo, config->colors.invalid.inner_ring);
					break;
				default:
					cairo_set_source_u32(window->cairo, config->colors.normal.inner_ring);
					break;
				}
				break;
			default:
				cairo_set_source_u32(window->cairo, config->colors.line);
				break;
			}
			// Draw inner + outer border of the circle
			cairo_set_line_width(window->cairo, 2.0);
			cairo_arc(window->cairo, wwidth/2, wheight/2, ARC_RADIUS - ARC_THICKNESS/2, 0, 2*M_PI);
			cairo_stroke(window->cairo);
			cairo_arc(window->cairo, wwidth/2, wheight/2, ARC_RADIUS + ARC_THICKNESS/2, 0, 2*M_PI);
			cairo_stroke(window->cairo);
		}
		window_render(window);
	}
}
