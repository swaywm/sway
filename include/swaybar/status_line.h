#ifndef _SWAYBAR_STATUS_LINE_H
#define _SWAYBAR_STATUS_LINE_H
#include <json-c/json.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "bar.h"

enum status_protocol {
	PROTOCOL_UNDEF,
	PROTOCOL_ERROR,
	PROTOCOL_TEXT,
	PROTOCOL_I3BAR,
};

struct i3bar_block {
	struct wl_list link;
	int ref_count;
	char *full_text, *short_text, *align;
	bool urgent;
	uint32_t *color;
	int min_width;
	char *name, *instance;
	bool separator;
	int separator_block_width;
	bool markup;
	// Airblader features
	uint32_t background;
	uint32_t border;
	int border_top;
	int border_bottom;
	int border_left;
	int border_right;
};

struct status_line {
	pid_t pid;
	int read_fd, write_fd;
	FILE *read, *write;

	enum status_protocol protocol;
	const char *text;
	struct wl_list blocks; // i3bar_block::link

	bool click_events;
	char *buffer;
	size_t buffer_size;
	size_t buffer_index;
	bool started;
	bool expecting_comma;
	json_tokener *tokener;
};

struct status_line *status_line_init(char *cmd);
void status_error(struct status_line *status, const char *text);
bool status_handle_readable(struct status_line *status);
void status_line_free(struct status_line *status);
bool i3bar_handle_readable(struct status_line *status);
enum hotspot_event_handling i3bar_block_send_click(struct status_line *status,
		struct i3bar_block *block, int x, int y, enum x11_button button);
void i3bar_block_unref(struct i3bar_block *block);
enum x11_button wl_button_to_x11_button(uint32_t button);
enum x11_button wl_axis_to_x11_button(uint32_t axis, wl_fixed_t value);

#endif
