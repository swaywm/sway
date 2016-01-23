#ifndef _SWAYBAR_RENDER_H
#define _SWAYBAR_RENDER_H

#include "config.h"
#include "state.h"

/**
 * Render swaybar.
 */
void render(struct output *output, struct swaybar_config *config, struct status_line *line);

/**
 * Set window height and modify internal spacing accordingly.
 */
void set_window_height(struct window *window, int height);

#endif /* _SWAYBAR_RENDER_H */
