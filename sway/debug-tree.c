#include <pango/pangocairo.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/util/log.h>
#include "config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "cairo.h"
#include "config.h"
#include "pango.h"

static const char *layout_to_str(enum sway_container_layout layout) {
	switch (layout) {
	case L_HORIZ:
		return "L_HORIZ";
	case L_VERT:
		return "L_VERT";
	case L_STACKED:
		return "L_STACKED";
	case L_TABBED:
		return "L_TABBED";
	case L_FLOATING:
		return "L_FLOATING";
	case L_NONE:
	default:
		return "L_NONE";
	}
}

static int draw_container(cairo_t *cairo, struct sway_container *container,
		struct sway_container *focus, int x, int y) {
	int text_width, text_height;
	get_text_size(cairo, "monospace", &text_width, &text_height,
		1, false, "%s id:%zd '%s' %s %dx%d@%d,%d",
		container_type_to_str(container->type), container->id, container->name,
		layout_to_str(container->layout),
		container->width, container->height, container->x, container->y);
	cairo_save(cairo);
	cairo_rectangle(cairo, x + 2, y, text_width - 2, text_height);
	cairo_set_source_u32(cairo, 0xFFFFFFE0);
	cairo_fill(cairo);
	int height = text_height;
	if (container->children) {
		for (int i = 0; i < container->children->length; ++i) {
			struct sway_container *child = container->children->items[i];
			if (child->parent == container) {
				cairo_set_source_u32(cairo, 0x000000FF);
			} else {
				cairo_set_source_u32(cairo, 0xFF0000FF);
			}
			height += draw_container(cairo, child, focus, x + 10, y + height);
		}
	}
	cairo_set_source_u32(cairo, 0xFFFFFFE0);
	cairo_rectangle(cairo, x, y, 2, height);
	cairo_fill(cairo);
	cairo_restore(cairo);
	cairo_move_to(cairo, x, y);
	if (focus == container) {
		cairo_set_source_u32(cairo, 0x0000FFFF);
	}
	pango_printf(cairo, "monospace", 1, false, "%s id:%zd '%s' %s %dx%d@%d,%d",
		container_type_to_str(container->type), container->id, container->name,
		layout_to_str(container->layout),
		container->width, container->height, container->x, container->y);
	return height;
}

bool enable_debug_tree = false;

void update_debug_tree() {
	if (!enable_debug_tree) {
		return;
	}

	int width = 640, height = 480;
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *container = root_container.children->items[i];
		if (container->width > width) {
			width = container->width;
		}
		if (container->height > height) {
			height = container->height;
		}
	}
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	PangoContext *pango = pango_cairo_create_context(cairo);

	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input_manager->seats, link) {
		break;
	}

	struct sway_container *focus = NULL;
	if (seat != NULL) {
		focus = seat_get_focus(seat);
	}
	cairo_set_source_u32(cairo, 0x000000FF);
	draw_container(cairo, &root_container, focus, 0, 0);

	cairo_surface_flush(surface);
	struct wlr_renderer *renderer = wlr_backend_get_renderer(server.backend);
	if (root_container.sway_root->debug_tree) {
		wlr_texture_destroy(root_container.sway_root->debug_tree);
	}
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888, stride, width, height, data);
	root_container.sway_root->debug_tree = texture;
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
}
