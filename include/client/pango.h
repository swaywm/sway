#ifndef _SWAY_CLIENT_PANGO_H
#define _SWAY_CLIENT_PANGO_H

#include "client/window.h"
#include "client/buffer.h"
#include <stdarg.h>

PangoLayout *get_pango_layout(struct window *window, struct buffer *buffer, const char *text);
void get_text_size(struct window *window, int *width, int *height, const char *fmt, ...);
void pango_printf(struct window *window, const char *fmt, ...);

#endif
