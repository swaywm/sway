#define _POSIX_C_SOURCE 200809L
#include <drm_fourcc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/interfaces/wlr_buffer.h>
#include "cairo_util.h"
#include "pango.h"
#include "sway/sway_text_buffer.h"
#include "sway/config.h"
#include "log.h"

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

	float scale;
	enum wl_output_subpixel subpixel;

	struct wl_list outputs; // text_buffer_output.link

	struct wl_listener output_enter;
	struct wl_listener output_leave;
	struct wl_listener destroy;
};

struct text_buffer_output {
	struct wl_list link;
	struct wlr_output *output;
	struct text_buffer *text_buffer;

	struct wl_listener commit;
};

static int get_text_width(struct sway_text_node *props) {
	if (props->max_width) {
		return MIN(props->max_width, props->width);
	}

	return props->width;
}

static void update_source_box(struct text_buffer *buffer) {
	struct sway_text_node *props = &buffer->props;
	struct wlr_fbox source_box = {
		.x = 0,
		.y = 0,
		.width = ceil(get_text_width(props) * buffer->scale),
		.height = ceil(props->height * buffer->scale),
	};

	wlr_scene_buffer_set_source_box(buffer->buffer_node, &source_box);
}

static void render_backing_buffer(struct text_buffer *buffer) {
	float scale = buffer->scale;
	int width = ceil(buffer->props.width * scale);
	int height = ceil(buffer->props.height * scale);
	float *color = (float *) &buffer->props.color;
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

	cairo_t *cairo = cairo_create(surface);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	pango = pango_cairo_create_context(cairo);
	cairo_set_source_rgba(cairo, color[0], color[1], color[2], color[3]);
	cairo_move_to(cairo, 0, (config->font_baseline - buffer->props.baseline) * scale);

	render_text(cairo, config->font_description, scale, buffer->props.pango_markup,
		"%s", buffer->text);

	cairo_surface_flush(surface);

	struct cairo_buffer *cairo_buffer = calloc(1, sizeof(struct cairo_buffer));
	wlr_buffer_init(&cairo_buffer->base, &cairo_buffer_impl, width, height);
	cairo_buffer->surface = surface;
	cairo_buffer->cairo = cairo;

	wlr_scene_buffer_set_buffer(buffer->buffer_node, &cairo_buffer->base);
	wlr_buffer_drop(&cairo_buffer->base);
	update_source_box(buffer);

err: 
	if (pango) g_object_unref(pango);
	cairo_font_options_destroy(fo);
}

static void ensure_backing_buffer(struct text_buffer *buffer) {
	float scale = 0;
	enum wl_output_subpixel subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;

	struct text_buffer_output *output;
	wl_list_for_each(output, &buffer->outputs, link) {
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

	// no outputs
	if (scale == 0) {
		return;
	}

	if (scale != buffer->scale || subpixel != buffer->subpixel) {
		buffer->scale = scale;
		buffer->subpixel = subpixel;
		render_backing_buffer(buffer);
	}
}

static void handle_output_commit(struct wl_listener *listener, void *data) {
	struct text_buffer_output *output = wl_container_of(listener, output, commit);
	struct wlr_output_event_commit *event = data;

	if (event->committed & (WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_SUBPIXEL)) {
		ensure_backing_buffer(output->text_buffer);
	}
}

static void handle_output_enter(struct wl_listener *listener, void *data) {
	struct text_buffer *buffer = wl_container_of(listener, buffer, output_enter);
	struct wlr_scene_output *output = data;
	struct text_buffer_output *buffer_output =
		calloc(1, sizeof(struct text_buffer_output));
	if (!buffer_output) {
		return;
	}

	buffer_output->text_buffer = buffer;

	buffer_output->commit.notify = handle_output_commit;
	wl_signal_add(&output->output->events.commit, &buffer_output->commit);

	buffer_output->output = output->output;
	wl_list_insert(&buffer->outputs, &buffer_output->link);
	ensure_backing_buffer(buffer);
}

static void text_buffer_output_destroy(struct text_buffer_output *output) {
	if (!output) {
		return;
	}

	wl_list_remove(&output->link);
	wl_list_remove(&output->commit.link);
	free(output);
}

static struct text_buffer_output *get_text_output_from_wlr_output(
		struct text_buffer *buffer, struct wlr_output *wlr_output) {
	struct text_buffer_output *output;
	wl_list_for_each(output, &buffer->outputs, link) {
		if (output->output == wlr_output) {
			return output;
		}
	}
	return NULL;
}

static void handle_output_leave(struct wl_listener *listener, void *data) {
	struct text_buffer *buffer = wl_container_of(listener, buffer, output_leave);
	struct wlr_scene_output *scene_output = data;

	struct text_buffer_output *output = get_text_output_from_wlr_output(
		buffer, scene_output->output);
	text_buffer_output_destroy(output);
	ensure_backing_buffer(buffer);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct text_buffer *buffer = wl_container_of(listener, buffer, destroy);

	wl_list_remove(&buffer->output_enter.link);
	wl_list_remove(&buffer->output_leave.link);
	wl_list_remove(&buffer->destroy.link);

	struct text_buffer_output *output, *tmp_output;
	wl_list_for_each_safe(output, tmp_output, &buffer->outputs, link) {
		text_buffer_output_destroy(output);
	}

	free(buffer->text);
	free(buffer);
}

static void text_calc_size(struct text_buffer *buffer) {
	struct sway_text_node *props = &buffer->props;

	cairo_t *c = cairo_create(NULL);
	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	get_text_size(c, config->font_description, &props->width, NULL,
		&props->baseline, 1, props->pango_markup, "%s", buffer->text);
	cairo_destroy(c);

	wlr_scene_buffer_set_dest_size(buffer->buffer_node,
		get_text_width(props), props->height);
}

struct sway_text_node *sway_text_node_create(struct wlr_scene_tree *parent,
		char *text, const float *color, bool pango_markup) {
	struct text_buffer *buffer = calloc(1, sizeof(struct text_buffer));
	if (buffer == NULL) {
		return NULL;
	}

	struct wlr_scene_buffer *node = wlr_scene_buffer_create(parent, NULL);
	if (!node) {
		free(buffer);
		return NULL;
	}

	buffer->buffer_node = node;
	buffer->props.node = &node->node;
	buffer->text = strdup(text);
	if (!buffer->text) {
		free(buffer);
		wlr_scene_node_destroy(&node->node);
		return NULL;
	}

	wl_list_init(&buffer->outputs);

	buffer->props.height = config->font_height;
	buffer->props.pango_markup = pango_markup;
	memcpy(&buffer->props.color, color, sizeof(float) * 4);

	buffer->destroy.notify = handle_destroy;
	wl_signal_add(&node->node.events.destroy, &buffer->destroy);
	buffer->output_enter.notify = handle_output_enter;
	wl_signal_add(&node->events.output_enter, &buffer->output_enter);
	buffer->output_leave.notify = handle_output_leave;
	wl_signal_add(&node->events.output_leave, &buffer->output_leave);

	text_calc_size(buffer);

	return &buffer->props;
}

void sway_text_node_set_color(struct sway_text_node *node, const float *color) {
	if (memcmp(&node->color, color, sizeof(float) * 4) == 0) {
		return;
	}

	memcpy(&node->color, color, sizeof(float) * 4);
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

void sway_text_node_set_max_width(struct sway_text_node *node, int max_width) {
	struct text_buffer *buffer = wl_container_of(node, buffer, props);
	buffer->props.max_width = max_width;
	wlr_scene_buffer_set_dest_size(buffer->buffer_node,
		get_text_width(&buffer->props), buffer->props.height);
	update_source_box(buffer);
}
