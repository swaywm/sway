#include "wayland-swaylock-client-protocol.h"
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <security/pam_appl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include "client/window.h"
#include "client/registry.h"
#include "client/cairo.h"
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

void sway_terminate(void) {
	int i;
	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	registry_teardown(registry);
	exit(EXIT_FAILURE);
}

char *password;
struct pam_response *pam_reply;

int function_conversation(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr) {  
	*resp = pam_reply;
	return PAM_SUCCESS;  
}  

/**
 * password will be zeroed out.
 */
bool verify_password(char *password) {
	struct passwd *passwd = getpwuid(getuid());
	char *username = passwd->pw_name;

	const struct pam_conv local_conversation = { function_conversation, NULL };
	pam_handle_t *local_auth_handle = NULL;
	int pam_err;
	if ((pam_err = pam_start("swaylock", username, &local_conversation, &local_auth_handle)) != PAM_SUCCESS) {
		sway_abort("PAM returned %d\n", pam_err);
	}
	pam_reply = (struct pam_response *)malloc(sizeof(struct pam_response));
	pam_reply[0].resp = password;
	pam_reply[0].resp_retcode = 0;
	if ((pam_err = pam_authenticate(local_auth_handle, 0)) != PAM_SUCCESS) {
		memset(password, 0, strlen(password));
		return false;
	}
	if ((pam_err = pam_end(local_auth_handle, pam_err)) != PAM_SUCCESS) {
		memset(password, 0, strlen(password));
		return false;
	}
	memset(password, 0, strlen(password));
	return true;
}

void notify_key(enum wl_keyboard_key_state state, xkb_keysym_t sym, uint32_t code, uint32_t codepoint) {
	sway_log(L_INFO, "notified of key %c", (char)codepoint);
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		switch (sym) {
		case XKB_KEY_Return:
			if (verify_password(password)) {
				exit(0);
			}
			break;
		default:
		{
			int i = strlen(password);
			password[i] = (char)codepoint;
			password[i + 1] = '\0';
			sway_log(L_INFO, "pw: %s", password);
			break;
		}
		}
	}
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	password = malloc(1024); // TODO: Let this grow
	password[0] = '\0';
	surfaces = create_list();
	registry = registry_poll();

	/*if (!registry->swaylock) {
		sway_abort("swaylock requires the compositor to support the swaylock extension.");
	}*/

	int i;
	for (i = 0; i < registry->outputs->length; ++i) {
		struct output_state *output = registry->outputs->items[i];
		struct window *window = window_setup(registry, output->width, output->height, true);
		if (!window) {
			sway_abort("Failed to create surfaces.");
		}
		//lock_set_lock_surface(registry->swaylock, output->output, window->surface);
		list_add(surfaces, window);
	}

	registry->input->notify = notify_key;

	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(argv[1], &err); // TODO: Parse i3lock arguments
	if (!pixbuf) {
		sway_abort("Failed to load background image.");
	}
	cairo_surface_t *image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	if (!image) {
		sway_abort("Failed to read background image.");
	}
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	const char *scaling_mode_str = argv[2];
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

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		if (window_prerender(window) && window->cairo) {
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
			default:
				sway_abort("Scaling mode '%s' not implemented yet!", scaling_mode_str);
			}

			cairo_paint(window->cairo);

			window_render(window);
		}
	}

	cairo_surface_destroy(image);

	while (wl_display_dispatch(registry->display) != -1);

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	registry_teardown(registry);

	return 0;
}
