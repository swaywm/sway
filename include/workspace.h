#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include <wlc/wlc.h>
#include "list.h"
#include "layout.h"

extern char *prev_workspace_name;

char *workspace_next_name(const char *output_name);
swayc_t *workspace_create(const char*);
swayc_t *workspace_by_name(const char*);
swayc_t *workspace_by_number(const char*);
bool workspace_switch(swayc_t*);
swayc_t *workspace_output_next();
swayc_t *workspace_next();
swayc_t *workspace_output_prev();
swayc_t *workspace_prev();

#endif
