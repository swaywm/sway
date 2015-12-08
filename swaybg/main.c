#include "wayland-desktop-shell-client-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <time.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "client/window.h"
#include "client/registry.h"
#include "log.h"
#include "list.h"

list_t *surfaces;

struct registry *registry;

enum scaling_mode {
	SCALING_MODE_STRETCH,
	SCALING_MODE_FILL,
	SCALING_MODE_FIT,
	SCALING_MODE_CENTER,
	SCALING_MODE_TILE,
};


#ifndef GDK_PIXBUF_CHECK_VERSION
#define GDK_PIXBUF_CHECK_VERSION(major,minor,micro) \
	(GDK_PIXBUF_MAJOR > (major) || \
		(GDK_PIXBUF_MAJOR == (major) && GDK_PIXBUF_MINOR > (minor)) || \
		(GDK_PIXBUF_MAJOR == (major) && GDK_PIXBUF_MINOR == (minor) && \
			GDK_PIXBUF_MICRO >= (micro)))
#endif

cairo_surface_t* __gdk_cairo_image_surface_create_from_pixbuf(const GdkPixbuf *gdkbuf)
{

	int chan = gdk_pixbuf_get_n_channels(gdkbuf);
	if (chan < 3) return NULL;

#if GDK_PIXBUF_CHECK_VERSION(2,32,0)
	const guint8* gdkpix = gdk_pixbuf_read_pixels(gdkbuf);
#else
	const guint8* gdkpix = gdk_pixbuf_get_pixels(gdkbuf);
#endif
	if (!gdkpix) {
		return NULL;
	}
	gint w = gdk_pixbuf_get_width(gdkbuf);
	gint h = gdk_pixbuf_get_height(gdkbuf);
	int stride = gdk_pixbuf_get_rowstride(gdkbuf);

	cairo_format_t fmt = (chan == 3) ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32;
	cairo_surface_t * cs = cairo_image_surface_create (fmt, w, h);
	cairo_surface_flush (cs);
	if ( !cs || cairo_surface_status(cs) != CAIRO_STATUS_SUCCESS) {
		return NULL;
	}

	int cstride = cairo_image_surface_get_stride(cs);
	unsigned char * cpix = cairo_image_surface_get_data(cs);

	if (chan == 3) {
		int i;
		for (i = h; i; --i) {
			const guint8 *gp = gdkpix;
			unsigned char *cp = cpix;
			const guint8* end = gp + 3*w;
			while (gp < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				cp[0] = gp[2];
				cp[1] = gp[1];
				cp[2] = gp[0];
#else
				cp[1] = gp[0];
				cp[2] = gp[1];
				cp[3] = gp[2];
#endif
				gp += 3;
				cp += 4;
			}
			gdkpix += stride;
			cpix += cstride;
		}
	} else {
		/* premul-color = alpha/255 * color/255 * 255 = (alpha*color)/255
		 * (z/255) = z/256 * 256/255     = z/256 (1 + 1/255)
		 *         = z/256 + (z/256)/255 = (z + z/255)/256
		 *         # recurse once
		 *         = (z + (z + z/255)/256)/256
		 *         = (z + z/256 + z/256/255) / 256
		 *         # only use 16bit uint operations, loose some precision,
		 *         # result is floored.
		 *       ->  (z + z>>8)>>8
		 *         # add 0x80/255 = 0.5 to convert floor to round
		 *       =>  (z+0x80 + (z+0x80)>>8 ) >> 8
		 * ------
		 * tested as equal to lround(z/255.0) for uint z in [0..0xfe02]
		 */
#define PREMUL_ALPHA(x,a,b,z) G_STMT_START { z = a * b + 0x80; x = (z + (z >> 8)) >> 8; } G_STMT_END
		int i;
		for (i = h; i; --i) {
			const guint8 *gp = gdkpix;
			unsigned char *cp = cpix;
			const guint8* end = gp + 4*w;
			guint z1, z2, z3;
			while (gp < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				PREMUL_ALPHA(cp[0], gp[2], gp[3], z1);
				PREMUL_ALPHA(cp[1], gp[1], gp[3], z2);
				PREMUL_ALPHA(cp[2], gp[0], gp[3], z3);
				cp[3] = gp[3];
#else
				PREMUL_ALPHA(cp[1], gp[0], gp[3], z1);
				PREMUL_ALPHA(cp[2], gp[1], gp[3], z2);
				PREMUL_ALPHA(cp[3], gp[2], gp[3], z3);
				cp[0] = gp[3];
#endif
				gp += 4;
				cp += 4;
			}
			gdkpix += stride;
			cpix += cstride;
		}
#undef PREMUL_ALPHA
	}
	cairo_surface_mark_dirty(cs);
	return cs;
}

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

int main(int argc, const char **argv) {
	init_log(L_INFO);
	surfaces = create_list();
	registry = registry_poll();

	if (argc != 4) {
		sway_abort("Do not run this program manually. See man 5 sway and look for output options.");
	}

	if (!registry->desktop_shell) {
		sway_abort("swaybg requires the compositor to support the desktop-shell extension.");
	}

	int desired_output = atoi(argv[1]);
	sway_log(L_INFO, "Using output %d of %d", desired_output, registry->outputs->length);
	int i;
	struct output_state *output = registry->outputs->items[desired_output];
	struct window *window = window_setup(registry, output->width, output->height, false);
	if (!window) {
		sway_abort("Failed to create surfaces.");
	}
	desktop_shell_set_background(registry->desktop_shell, output->output, window->surface);
	list_add(surfaces, window);

	GError *err=NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(argv[2],&err);
	if (!pixbuf) {
		sway_abort("Failed to load background image.");
	}
	cairo_surface_t *image=__gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	if (!image) {
		sway_abort("Failed to read background image.");
	}
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	const char *scaling_mode_str = argv[3];
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
