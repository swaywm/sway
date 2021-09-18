#ifndef _SWAYBAR_I3BAR_H
#define _SWAYBAR_I3BAR_H

#include "input.h"
#include "status_line.h"

struct i3bar_block {
	struct wl_list link; // status_link::blocks
	int ref_count;
	char *full_text, *short_text, *align, *min_width_str;
	bool urgent;
	uint32_t color;
	bool color_set;
	int min_width;
	char *name, *instance;
	bool separator;
	int separator_block_width;
	bool markup;
	// Airblader features
	uint32_t background;
	uint32_t border;
	bool border_set;
	int border_top;
	int border_bottom;
	int border_left;
	int border_right;
};

void i3bar_block_unref(struct i3bar_block *block);
bool i3bar_handle_readable(struct status_line *status);
enum hotspot_event_handling i3bar_block_send_click(struct status_line *status,
		struct i3bar_block *block, double x, double y, double rx, double ry,
		double w, double h, int scale, uint32_t button, bool released);

#endif
