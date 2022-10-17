#ifndef _SWAY_BUFFER_H
#define _SWAY_BUFFER_H
#include <wlr/types/wlr_scene.h>

struct sway_text_node {
	int width;
	int max_width;
	int height;
	int baseline;
	bool pango_markup;
	float color[4];

	struct wlr_scene_node *node;
};

struct sway_text_node *sway_text_node_create(struct wlr_scene_tree *parent,
		char *text, const float *color, bool pango_markup);

void sway_text_node_set_color(struct sway_text_node *node, const float *color);

void sway_text_node_set_text(struct sway_text_node *node, char *text);

void sway_text_node_set_max_width(struct sway_text_node *node, int max_width);

#endif
