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
#include "client/window.h"
#include "client/registry.h"
#include "client/cairo.h"
#include "ipc-client.h"
#include "log.h"

list_t *surfaces;
struct registry *registry;

enum scaling_mode {
	SCALING_MODE_STRETCH,
	SCALING_MODE_FILL,
	SCALING_MODE_FIT,
	SCALING_MODE_CENTER,
	SCALING_MODE_TILE,
};

void sway_terminate(int exit_code) {
	int i;
	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	if (registry) {
		registry_teardown(registry);
	}
	exit(exit_code);
}

char *password;
int password_size;

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
 * password will be zeroed out.
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
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		switch (sym) {
		case XKB_KEY_Return:
			if (verify_password()) {
				exit(0);
			}
			password_size = 1024;
			password = malloc(password_size);
			password[0] = '\0';
			break;
		case XKB_KEY_BackSpace:
			{
				int i = strlen(password);
				if (i > 0) {
					password[i - 1] = '\0';
				}
				break;
			}
		default:
			{
				int i = strlen(password);
				if (i + 1 == password_size) {
					password_size += 1024;
					password = realloc(password, password_size);
				}
				password[i] = (char)codepoint;
				password[i + 1] = '\0';
				break;
			}
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

	switch (scaling_mode) {
	case SCALING_MODE_STRETCH:
		cairo_scale(window->cairo,
				(double) window->width / width,
				(double) window->height / height);
		cairo_set_source_surface(window->cairo, image, 0, 0);
		break;
	case SCALING_MODE_FILL:
	{
		double window_ratio = (double) window->width / window->height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double) window->width / width;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					0,
					(double) window->height/2 / scale - height/2);
		} else {
			double scale = (double) window->height / height;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					(double) window->width/2 / scale - width/2,
					0);
		}
		break;
	}
	case SCALING_MODE_FIT:
	{
		double window_ratio = (double) window->width / window->height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double) window->height / height;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					(double) window->width/2 / scale - width/2,
					0);
		} else {
			double scale = (double) window->width / width;
			cairo_scale(window->cairo, scale, scale);
			cairo_set_source_surface(window->cairo, image,
					0,
					(double) window->height/2 / scale - height/2);
		}
		break;
	}
	case SCALING_MODE_CENTER:
		cairo_set_source_surface(window->cairo, image,
				(double) window->width/2 - width/2,
				(double) window->height/2 - height/2);
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
		fprintf(stderr, "GDK Error: %s\n", err->message);
		sway_abort("Failed to load background image.");
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
	char *scaling_mode_str = "fit", *socket_path = NULL;
	int i, num_images = 0, color_set = 0;
	uint32_t color = 0xFFFFFFFF;
	void *images;

	init_log(L_INFO);

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"color", required_argument, NULL, 'c'},
		{"image", required_argument, NULL, 'i'},
		{"scaling", required_argument, NULL, 's'},
		{"tiling", no_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaylock [options...]\n"
		"\n"
		"  -h, --help                      Show help message and quit.\n"
		"  -c, --color <rrggbb[aa]>        Turn the screen into the given color instead of white.\n"
		"  -s, --scaling                   Scaling mode: stretch, fill, fit, center, tile.\n"
		"  -t, --tiling                    Same as --scaling=tile.\n"
		"  -v, --version                   Show the version number and quit.\n"
		"  -i, --image [<display>:]<path>  Display the given image.\n"
		"  --socket <socket>               Use the specified socket.\n";

	registry = registry_poll();

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hc:i:s:tv", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'c': 
		{
			int colorlen = strlen(optarg);
			if (colorlen < 6 || colorlen == 7 || colorlen > 8) {
				fprintf(stderr, "color must be specified in 3 or 4 byte format, e.g. ff0000 or ff0000ff\n");
				exit(EXIT_FAILURE);
			}
			color = strtol(optarg, NULL, 16);
			color_set = 1;

			if (colorlen == 6) {
				color <<= 8;
				color |= 0xFF;
			}
			sway_log(L_DEBUG, "color: 0x%x", color);
			break;
		}
		case 'i':
		{
			char *image_path = strchr(optarg, ':');
			if (image_path == NULL) {
				if (num_images == 0) {
					images = load_image(optarg);
					num_images = -1;
				} else {
					fprintf(stderr, "display must be defined for all --images or no --images.\n");
					exit(EXIT_FAILURE);
				}
			} else {
				if (num_images == 0) {
					images = calloc(registry->outputs->length, sizeof(char*) * 2);
				} else if (num_images == -1) {
					fprintf(stderr, "display must be defined for all --images or no --images.\n");
					exit(EXIT_FAILURE);
				}

				image_path[0] = '\0';
				((char**) images)[num_images * 2] = optarg;
				((char**) images)[num_images++ * 2 + 1] = ++image_path;
			}
			break;
		}
		case 's':
			scaling_mode_str = optarg;
			break;
		case 't':
			scaling_mode_str = "tile";
			break;
		case 'p':
			socket_path = optarg;
			break;
		case 'v':
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "swaylock version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version not detected\n");
#endif
			exit(EXIT_SUCCESS);
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	enum scaling_mode scaling_mode = SCALING_MODE_STRETCH;
	if (strcmp(scaling_mode_str, "stretch") == 0) {
		scaling_mode = SCALING_MODE_STRETCH;
	} else if (strcmp(scaling_mode_str, "fill") == 0) {
		scaling_mode = SCALING_MODE_FILL;
	} else if (strcmp(scaling_mode_str, "fit") == 0) {
		scaling_mode = SCALING_MODE_FIT;
	} else if (strcmp(scaling_mode_str, "center") == 0) {
		scaling_mode = SCALING_MODE_CENTER;
	} else if (strcmp(scaling_mode_str, "tile") == 0) {
		scaling_mode = SCALING_MODE_TILE;
	} else {
		sway_abort("Unsupported scaling mode: %s", scaling_mode_str);
	}

	password_size = 1024;
	password = malloc(password_size);
	password[0] = '\0';
	surfaces = create_list();
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
		struct window *window = window_setup(registry, output->width, output->height, true);
		if (!window) {
			sway_abort("Failed to create surfaces.");
		}
		list_add(surfaces, window);
	}

	registry->input->notify = notify_key;

	if (num_images >= 1) {
		char **displays_paths = images;
		images = calloc(registry->outputs->length, sizeof(cairo_surface_t*));

		int socketfd = ipc_open_socket(socket_path);
		uint32_t len = 0;
		char *outputs = ipc_single_command(socketfd, IPC_GET_OUTPUTS, "", &len);
		struct json_object *json_outputs = json_tokener_parse(outputs);

		for (i = 0; i < registry->outputs->length; ++i) {
			if (displays_paths[i * 2] != NULL) {
				for (int j = 0;; ++j) {
					if (j >= json_object_array_length(json_outputs)) {
						fprintf(stderr, "%s is not an extant display\n", displays_paths[i * 2]);
						exit(EXIT_FAILURE);
					}

					struct json_object *dsp_name, *at_j = json_object_array_get_idx(json_outputs, j);
					if (!json_object_object_get_ex(at_j, "name", &dsp_name)) {
						sway_abort("display doesn't have a name field");
					}
					if (!strcmp(displays_paths[i * 2], json_object_get_string(dsp_name))) {
						((cairo_surface_t**) images)[j] = load_image(displays_paths[i * 2 + 1]);
						break;
					}
				}
			}
		}

		json_object_put(json_outputs);
		close(socketfd);
		free(displays_paths);
	}

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		if (!window_prerender(window) || !window->cairo) {
			continue;
		}

		if (num_images == 0 || color_set) {
			render_color(window, color);
		}

		if (num_images == -1) {
			render_image(window, images, scaling_mode);
		} else if (num_images >= 1) {
			if (((cairo_surface_t**) images)[i] != NULL) {
				render_image(window, ((cairo_surface_t**) images)[i], scaling_mode);
			}
		}

		window_render(window);
	}

	if (num_images == -1) {
		cairo_surface_destroy((cairo_surface_t*) images);
	} else if (num_images >= 1) {
		for (i = 0; i < registry->outputs->length; ++i) {
			if (((cairo_surface_t**) images)[i] != NULL) {
				cairo_surface_destroy(((cairo_surface_t**) images)[i]);
			}
		}
		free(images);
	}

	bool locked = false;
	while (wl_display_dispatch(registry->display) != -1) {
		if (!locked) {
			for (i = 0; i < registry->outputs->length; ++i) {
				struct output_state *output = registry->outputs->items[i];
				struct window *window = surfaces->items[i];
				lock_set_lock_surface(registry->swaylock, output->output, window->surface);
			}
			locked = true;
		}
	}

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	registry_teardown(registry);

	return 0;
}
