#ifndef _SWAYBAR_STATUS_LINE_H
#define _SWAYBAR_STATUS_LINE_H
#include <json.h>
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

struct status_line {
	struct swaybar *bar;

	pid_t pid;
	int read_fd, write_fd;
	FILE *read, *write;

	enum status_protocol protocol;
	const char *text;
	struct wl_list blocks; // i3bar_block::link

	int stop_signal;
	int cont_signal;

	bool click_events;
	bool float_event_coords;
	bool clicked;
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

#endif
