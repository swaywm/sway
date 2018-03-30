#ifndef _SWAYBAR_STATUS_LINE_H
#define _SWAYBAR_STATUS_LINE_H
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "bar.h"

enum status_protocol {
	PROTOCOL_UNDEF,
	PROTOCOL_TEXT,
	PROTOCOL_I3BAR,
};

struct status_line {
	pid_t pid;
	int read_fd, write_fd;
	FILE *read, *write;

	enum status_protocol protocol;
	const char *text;

	char *buffer;
	size_t buffer_size;
};

struct status_line *status_line_init(char *cmd);
void status_line_free(struct status_line *status);
bool handle_status_readable(struct status_line *status);

#endif
