#ifndef _SWAYBAR_STATUS_LINE_H
#define _SWAYBAR_STATUS_LINE_H
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

struct text_protocol_state {
	char *buffer;
	size_t buffer_size;
};

enum json_node_type {
	JSON_NODE_UNKNOWN,
	JSON_NODE_ARRAY,
	JSON_NODE_STRING,
};

struct i3bar_protocol_state {
	bool click_events;
	char *buffer;
	size_t buffer_size;
	size_t buffer_index;
	const char *current_node;
	bool escape;
	size_t depth;
	enum json_node_type nodes[16];
};

struct i3bar_block {
	struct wl_list link;
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

	struct text_protocol_state text_state;
	struct i3bar_protocol_state i3bar_state;
};

struct status_line *status_line_init(char *cmd);
void status_error(struct status_line *status, const char *text);
bool status_handle_readable(struct status_line *status);
void status_line_free(struct status_line *status);
bool i3bar_handle_readable(struct status_line *status);
void i3bar_block_send_click(struct status_line *status,
		struct i3bar_block *block, int x, int y, enum x11_button button);
void i3bar_block_free(struct i3bar_block *block);
enum x11_button wl_button_to_x11_button(uint32_t button);
enum x11_button wl_axis_to_x11_button(uint32_t axis, wl_fixed_t value);

#endif
