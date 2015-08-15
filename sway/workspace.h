#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include <wlc/wlc.h>
#include "list.h"
#include "layout.h"

extern swayc_t *active_workspace;

char *workspace_next_name(void);
swayc_t *workspace_create(const char*);
swayc_t *workspace_find_by_name(const char*);
void workspace_switch(swayc_t*);
void layout_log(const swayc_t *c, int depth);

#endif
