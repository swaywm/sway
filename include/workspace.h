#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include <wlc/wlc.h>
#include "list.h"
#include "layout.h"

extern char *prev_workspace_name;

// Search for available workspace name on output from config
const char *workspace_output_open_name(swayc_t *output);
// Search for any available workspace name
const char *workspace_next_name(void);


swayc_t *workspace_by_name(const char*);
void workspace_switch(swayc_t*);
swayc_t *workspace_output_next(void);
swayc_t *workspace_next(void);
swayc_t *workspace_output_prev(void);
swayc_t *workspace_prev(void);

#endif
