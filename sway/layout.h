#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlc/wlc.h>
#include "list.h"

typedef enum {
    LAYOUT_IS_VIEW,
    LAYOUT_TILE_HORIZ,
    LAYOUT_TILE_VERT,
    LAYOUT_TABBED,
    LAYOUT_STACKED
} container_layout_t;

struct sway_container {
    wlc_handle output;
    list_t *children;
    container_layout_t layout;
    struct sway_container *parent;
};

extern list_t *outputs;
extern wlc_handle focused_view;

void init_layout();
void add_output(wlc_handle output);
void destroy_output(wlc_handle output);
wlc_handle get_topmost(wlc_handle output, size_t offset);
void destroy_view(wlc_handle view);
void add_view(wlc_handle view);

#endif
