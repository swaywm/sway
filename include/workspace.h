#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include <wlc/wlc.h>
#include "list.h"
#include "layout.h"

char *workspace_next_name(void);
swayc_t *workspace_create(const char*);
swayc_t *workspace_by_name(const char*);
void workspace_switch(swayc_t*);
swayc_t *workspace_output_next();
swayc_t *workspace_next();
swayc_t *workspace_output_prev();
swayc_t *workspace_prev();

#endif
