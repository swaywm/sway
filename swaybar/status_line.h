#ifndef _SWAYBAR_STATUS_LINE_H
#define _SWAYBAR_STATUS_LINE_H

#include <stdint.h>
#include <stdbool.h>

#include "list.h"
#include "state.h"

typedef enum {UNDEF, TEXT, I3BAR} command_protocol;

struct status_line {
	list_t *block_line;
	char *text_line;
	command_protocol protocol;
};

struct status_block {
	char *full_text, *short_text, *align;
	bool urgent;
	uint32_t color;
	int min_width;
	char *name, *instance;
	bool separator;
	int separator_block_width;
	// Airblader features
	uint32_t background;
	uint32_t border;
	int border_top;
	int border_bottom;
	int border_left;
	int border_right;
};

/**
 * Initialize status line struct.
 */
struct status_line *init_status_line();

/**
 * handle status line activity.
 */
bool handle_status_line(struct swaybar_state *st);

#endif /* _SWAYBAR_STATUS_LINE_H */
