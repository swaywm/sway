#include <pango/pangocairo.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/util/log.h>
#include "config.h"
#include "sway/debug.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "cairo.h"
#include "config.h"
#include "pango.h"

struct sway_debug debug;

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
	case L_NONE:
		return "L_NONE";
	}
	return "L_NONE";
}

static char *get_string(struct sway_node *node) {
	char *buffer = malloc(512);
	switch (node->type) {
	case N_ROOT:
		snprintf(buffer, 512, "N_ROOT id:%zd %.fx%.f@%.f,%.f", node->id,
				root->width, root->height, root->x, root->y);
		break;
	case N_OUTPUT:
		snprintf(buffer, 512, "N_OUTPUT id:%zd '%s' %dx%d@%d,%d", node->id,
				node->sway_output->wlr_output->name,
				node->sway_output->width,
				node->sway_output->height,
				node->sway_output->lx,
				node->sway_output->ly);
		break;
	case N_WORKSPACE:
		snprintf(buffer, 512, "N_WORKSPACE id:%zd '%s' %s %dx%d@%.f,%.f",
				node->id, node->sway_workspace->name,
				layout_to_str(node->sway_workspace->layout),
				node->sway_workspace->width, node->sway_workspace->height,
				node->sway_workspace->x, node->sway_workspace->y);
		break;
	case N_CONTAINER:
		snprintf(buffer, 512, "N_CONTAINER id:%zd '%s' %s %.fx%.f@%.f,%.f",
				node->id, node->sway_container->title,
				layout_to_str(node->sway_container->layout),
				node->sway_container->width, node->sway_container->height,
				node->sway_container->x, node->sway_container->y);
		break;
	}
	return buffer;
}

static list_t *get_children(struct sway_node *node) {
	switch (node->type) {
	case N_ROOT:
		return root->outputs;
	case N_OUTPUT:
		return node->sway_output->workspaces;
	case N_WORKSPACE:
		return node->sway_workspace->tiling;
	case N_CONTAINER:
		return node->sway_container->children;
	}
	return NULL;
}

static int draw_node(cairo_t *cairo, struct sway_node *node,
		struct sway_node *focus, int x, int y) {
	int text_width, text_height;
	char *buffer = get_string(node);
	get_text_size(cairo, "monospace", &text_width, &text_height, NULL,
		1, false, buffer);
	cairo_save(cairo);
	cairo_rectangle(cairo, x + 2, y, text_width - 2, text_height);
	cairo_set_source_u32(cairo, 0xFFFFFFE0);
	cairo_fill(cairo);
	int height = text_height;
	list_t *children = get_children(node);
	if (children) {
		for (int i = 0; i < children->length; ++i) {
			// This is really dirty - the list contains specific structs but
			// we're casting them as nodes. This works because node is the first
			// item in each specific struct. This is acceptable because this is
			// debug code.
			struct sway_node *child = children->items[i];
			if (node_get_parent(child) == node) {
				cairo_set_source_u32(cairo, 0x000000FF);
			} else {
				cairo_set_source_u32(cairo, 0xFF0000FF);
			}
			height += draw_node(cairo, child, focus, x + 10, y + height);
		}
	}
	cairo_set_source_u32(cairo, 0xFFFFFFE0);
	cairo_rectangle(cairo, x, y, 2, height);
	cairo_fill(cairo);
	cairo_restore(cairo);
	cairo_move_to(cairo, x, y);
	if (focus == node) {
		cairo_set_source_u32(cairo, 0x0000FFFF);
	}
	pango_printf(cairo, "monospace", 1, false, buffer);
	free(buffer);
	return height;
}

void update_debug_tree(void) {
	if (!debug.render_tree) {
		return;
	}

	int width = 640, height = 480;
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if (output->width > width) {
			width = output->width;
		}
		if (output->height > height) {
			height = output->height;
		}
	}
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	PangoContext *pango = pango_cairo_create_context(cairo);

	struct sway_seat *seat = input_manager_current_seat();
	struct sway_node *focus = seat_get_focus(seat);

	cairo_set_source_u32(cairo, 0x000000FF);
	draw_node(cairo, &root->node, focus, 0, 0);

	cairo_surface_flush(surface);
	struct wlr_renderer *renderer = wlr_backend_get_renderer(server.backend);
	if (root->debug_tree) {
		wlr_texture_destroy(root->debug_tree);
	}
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888, stride, width, height, data);
	root->debug_tree = texture;
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
}
