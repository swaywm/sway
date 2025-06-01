#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/interfaces/wlr_buffer.h>
#include "cairo_util.h"
#include "log.h"
#include "pango.h"
#include "sway/config.h"
#include "sway/sway_text_node.h"

struct cairo_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
	cairo_t *cairo;
};

static void cairo_buffer_handle_destroy(struct wlr_buffer *wlr_buffer) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

	cairo_surface_destroy(buffer->surface);
	cairo_destroy(buffer->cairo);
	free(buffer);
}

static bool cairo_buffer_handle_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*data = cairo_image_surface_get_data(buffer->surface);
	*stride = cairo_image_surface_get_stride(buffer->surface);
	*format = DRM_FORMAT_ARGB8888;
	return true;
}

static void cairo_buffer_handle_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
	.destroy = cairo_buffer_handle_destroy,
	.begin_data_ptr_access = cairo_buffer_handle_begin_data_ptr_access,
	.end_data_ptr_access = cairo_buffer_handle_end_data_ptr_access,
};

struct text_buffer {
	struct wlr_scene_buffer *buffer_node;
	char *text;
	struct sway_text_node props;

	bool visible;
	float scale;
	enum wl_output_subpixel subpixel;

	struct wl_listener outputs_update;
	struct wl_listener destroy;
};

static double get_text_width(struct sway_text_node *props) {
	double width = props->width;
	if (props->max_width >= 0) {
		width = MIN(width, props->max_width);
	}
	return MAX(width, 0);
}

static void render_backing_buffer(struct text_buffer *buffer) {
	if (!buffer->visible) {
		return;
	}

	if (buffer->props.max_width == 0) {
		wlr_scene_buffer_set_buffer(buffer->buffer_node, NULL);
		return;
	}

	float scale = buffer->scale;
	int width = ceil(get_text_width(&buffer->props) * scale);
	int height = ceil(buffer->props.height * scale);
	float *color = (float *)&buffer->props.color;
	float *background = (float *)&buffer->props.background;
	PangoContext *pango = NULL;

	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	enum wl_output_subpixel subpixel = buffer->subpixel;
	if (subpixel == WL_OUTPUT_SUBPIXEL_NONE || subpixel == WL_OUTPUT_SUBPIXEL_UNKNOWN) {
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
	} else {
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
		cairo_font_options_set_subpixel_order(fo, to_cairo_subpixel_order(subpixel));
	}

	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, width, height);
	cairo_status_t status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		sway_log(SWAY_ERROR, "cairo_image_surface_create failed: %s",
			cairo_status_to_string(status));
		goto err;
	}

	struct cairo_buffer *cairo_buffer = calloc(1, sizeof(*cairo_buffer));
	if (!cairo_buffer) {
		sway_log(SWAY_ERROR, "cairo_buffer allocation failed");
		goto err;
	}

	cairo_t *cairo = cairo_create(surface);
	if (!cairo) {
		sway_log(SWAY_ERROR, "cairo_create failed");
		free(cairo_buffer);
		goto err;
	}

	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	pango = pango_cairo_create_context(cairo);

	cairo_set_source_rgba(cairo, background[0], background[1], background[2], background[3]);
	cairo_rectangle(cairo, 0, 0, width, height);
	cairo_fill(cairo);

	cairo_set_source_rgba(cairo, color[0], color[1], color[2], color[3]);
	cairo_move_to(cairo, 0, (config->font_baseline - buffer->props.baseline) * scale);

	render_text(cairo, config->font_description, scale, buffer->props.pango_markup,
		"%s", buffer->text);

	cairo_surface_flush(surface);

	wlr_buffer_init(&cairo_buffer->base, &cairo_buffer_impl, width, height);
	cairo_buffer->surface = surface;
	cairo_buffer->cairo = cairo;

	wlr_scene_buffer_set_buffer(buffer->buffer_node, &cairo_buffer->base);
	wlr_buffer_drop(&cairo_buffer->base);

	pixman_region64f_t opaque;
	pixman_region64f_init(&opaque);
	if (background[3] == 1) {
		pixman_region64f_union_rectf(&opaque, &opaque, 0, 0,
			get_text_width(&buffer->props), buffer->props.height);
	}
	wlr_scene_buffer_set_opaque_region(buffer->buffer_node, &opaque);
	pixman_region64f_fini(&opaque);

err:
	if (pango) g_object_unref(pango);
	cairo_font_options_destroy(fo);
}

static void handle_outputs_update(struct wl_listener *listener, void *data) {
	struct text_buffer *buffer = wl_container_of(listener, buffer, outputs_update);
	struct wlr_scene_outputs_update_event *event = data;

	float scale = 0;
	enum wl_output_subpixel subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;

	for (size_t i = 0; i < event->size; i++) {
		struct wlr_scene_output *output = event->active[i];
		if (subpixel == WL_OUTPUT_SUBPIXEL_UNKNOWN) {
			subpixel = output->output->subpixel;
		} else if (subpixel != output->output->subpixel) {
			subpixel = WL_OUTPUT_SUBPIXEL_NONE;
		}

		if (scale != 0 && scale != output->output->scale) {
			// drop down to gray scale if we encounter outputs with different
			// scales or else we will have chromatic aberations
			subpixel = WL_OUTPUT_SUBPIXEL_NONE;
		}

		if (scale < output->output->scale) {
			scale = output->output->scale;
		}
	}

	buffer->visible = event->size > 0;

	if (scale != buffer->scale || subpixel != buffer->subpixel) {
		buffer->scale = scale;
		buffer->subpixel = subpixel;
		render_backing_buffer(buffer);
	}
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct text_buffer *buffer = wl_container_of(listener, buffer, destroy);

	wl_list_remove(&buffer->outputs_update.link);
	wl_list_remove(&buffer->destroy.link);

	free(buffer->text);
	free(buffer);
}

static void text_calc_size(struct text_buffer *buffer) {
	struct sway_text_node *props = &buffer->props;

	cairo_t *c = cairo_create(NULL);
	if (!c) {
		sway_log(SWAY_ERROR, "cairo_t allocation failed");
		return;
	}

	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	get_text_size(c, config->font_description, &props->width, NULL,
		&props->baseline, 1, props->pango_markup, "%s", buffer->text);
	cairo_destroy(c);

	wlr_scene_buffer_set_dest_size(buffer->buffer_node,
		get_text_width(props), props->height);
}

struct sway_text_node *sway_text_node_create(struct wlr_scene_tree *parent,
		char *text, float color[4], bool pango_markup) {
	struct text_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		return NULL;
	}

	struct wlr_scene_buffer *node = wlr_scene_buffer_create(parent, NULL);
	if (!node) {
		free(buffer);
		return NULL;
	}

	buffer->buffer_node = node;
	buffer->props.node = &node->node;
	buffer->props.max_width = -1;
	buffer->text = strdup(text);
	if (!buffer->text) {
		free(buffer);
		wlr_scene_node_destroy(&node->node);
		return NULL;
	}

	buffer->props.height = config->font_height;
	buffer->props.pango_markup = pango_markup;
	memcpy(&buffer->props.color, color, sizeof(*color) * 4);

	buffer->destroy.notify = handle_destroy;
	wl_signal_add(&node->node.events.destroy, &buffer->destroy);
	buffer->outputs_update.notify = handle_outputs_update;
	wl_signal_add(&node->events.outputs_update, &buffer->outputs_update);

	text_calc_size(buffer);

	return &buffer->props;
}

void sway_text_node_set_color(struct sway_text_node *node, float color[4]) {
	if (memcmp(&node->color, color, sizeof(*color) * 4) == 0) {
		return;
	}

	memcpy(&node->color, color, sizeof(*color) * 4);
	struct text_buffer *buffer = wl_container_of(node, buffer, props);

	render_backing_buffer(buffer);
}

void sway_text_node_set_text(struct sway_text_node *node, char *text) {
	struct text_buffer *buffer = wl_container_of(node, buffer, props);
	if (strcmp(buffer->text, text) == 0) {
		return;
	}

	char *new_text = strdup(text);
	if (!new_text) {
		return;
	}

	free(buffer->text);
	buffer->text = new_text;

	text_calc_size(buffer);
	render_backing_buffer(buffer);
}

void sway_text_node_set_max_width(struct sway_text_node *node, double max_width) {
	struct text_buffer *buffer = wl_container_of(node, buffer, props);
	if (max_width == buffer->props.max_width) {
		return;
	}
	buffer->props.max_width = max_width;
	wlr_scene_buffer_set_dest_size(buffer->buffer_node,
		get_text_width(&buffer->props), buffer->props.height);
	render_backing_buffer(buffer);
}

void sway_text_node_set_background(struct sway_text_node *node, float background[4]) {
	struct text_buffer *buffer = wl_container_of(node, buffer, props);
	if (memcmp(&node->background, background, sizeof(*background) * 4) == 0) {
		return;
	}
	memcpy(&node->background, background, sizeof(*background) * 4);
	render_backing_buffer(buffer);
}
