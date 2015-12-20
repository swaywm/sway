#include <cairo/cairo.h>
#include "client/cairo.h"

#ifdef WITH_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifndef GDK_PIXBUF_CHECK_VERSION
#define GDK_PIXBUF_CHECK_VERSION(major,minor,micro) \
	(GDK_PIXBUF_MAJOR > (major) || \
		(GDK_PIXBUF_MAJOR == (major) && GDK_PIXBUF_MINOR > (minor)) || \
		(GDK_PIXBUF_MAJOR == (major) && GDK_PIXBUF_MINOR == (minor) && \
			GDK_PIXBUF_MICRO >= (micro)))
#endif

cairo_surface_t* gdk_cairo_image_surface_create_from_pixbuf(const GdkPixbuf *gdkbuf) {
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
#endif //WITH_GDK_PIXBUF
