#ifndef _SWAYBAR_STATUS_LINE_H
#define _SWAYBAR_STATUS_LINE_H

#include <stdint.h>
#include <stdbool.h>

#include "list.h"
#include "bar.h"

typedef enum {UNDEF, TEXT, I3BAR} command_protocol;

struct status_line {
	list_t *block_line;
	const char *text_line;
	command_protocol protocol;
	bool click_events;
};

struct status_block {
	char *full_text, *short_text, *align;
	bool urgent;
	uint32_t color;
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

	// Set during rendering
	int x;
	int width;
};

/**
 * Initialize status line struct.
 */
struct status_line *init_status_line();

/**
 * handle status line activity.
 */
bool handle_status_line(struct bar *bar);

/**
 * Handle mouse clicks.
 */
bool status_line_mouse_event(struct bar *bar, int x, int y, uint32_t button);

/**
 * Free status line struct.
 */
void free_status_line(struct status_line *line);

#endif /* _SWAYBAR_STATUS_LINE_H */
