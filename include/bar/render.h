#ifndef _SWAYBAR_RENDER_H
#define _SWAYBAR_RENDER_H

#include "config.h"
#include "bar.h"

/**
 * Render swaybar.
 */
void render(struct output *output, struct config *config, struct status_line *line);

/**
 * Set window height and modify internal spacing accordingly.
 */
void set_window_height(struct window *window, int height);

/**
 * Compute the size of a workspace name
 */
void workspace_button_size(struct window *window, const char *workspace_name, int *width, int *height);

#endif /* _SWAYBAR_RENDER_H */
