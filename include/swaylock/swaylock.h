#ifndef _SWAYLOCK_H
#define _SWAYLOCK_H

#include "client/cairo.h"

enum scaling_mode {
	SCALING_MODE_STRETCH,
	SCALING_MODE_FILL,
	SCALING_MODE_FIT,
	SCALING_MODE_CENTER,
	SCALING_MODE_TILE,
};

enum auth_state {
	AUTH_STATE_IDLE,
	AUTH_STATE_INPUT,
	AUTH_STATE_BACKSPACE,
	AUTH_STATE_VALIDATING,
	AUTH_STATE_INVALID,
};

enum line_source {
	LINE_SOURCE_DEFAULT,
	LINE_SOURCE_RING,
	LINE_SOURCE_INSIDE,
};

struct render_data {
	list_t *surfaces;
	// Output specific images
	cairo_surface_t **images;
	// OR one image for all outputs:
	cairo_surface_t *image;
	int num_images;
	int color_set;
	uint32_t color;
	enum scaling_mode scaling_mode;
	enum auth_state auth_state;
};

struct lock_colors {
	uint32_t inner_ring;
	uint32_t outer_ring;
};

struct lock_config {
	char *font;

	struct {
		uint32_t text;
		uint32_t line;
		uint32_t separator;
		uint32_t input_cursor;
		uint32_t backspace_cursor;
		struct lock_colors normal;
		struct lock_colors validating;
		struct lock_colors invalid;
	} colors;

	int radius;
	int thickness;
};

void render(struct render_data* render_data, struct lock_config *config);

#endif
