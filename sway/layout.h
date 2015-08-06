#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlc/wlc.h>
#include "list.h"

typedef enum {
    LAYOUT_TILE_HORIZ,
    LAYOUT_TILE_VERT,
    LAYOUT_TABBED,
    LAYOUT_STACKED
} container_layout_t;

struct sway_container {
    wlc_handle output;
    list_t *children;
    container_layout_t layout;
};

extern list_t *outputs;

void init_layout();
void add_output(wlc_handle output);
wlc_handle get_topmost(wlc_handle output, size_t offset);

#endif
