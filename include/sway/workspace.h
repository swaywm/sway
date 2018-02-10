#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include <sway/container.h>

extern char *prev_workspace_name;

char *workspace_next_name(const char *output_name);
swayc_t *workspace_create(const char *name);
bool workspace_switch(swayc_t *workspace);

struct sway_container *workspace_by_number(const char* name);
swayc_t *workspace_by_name(const char*);

struct sway_container *workspace_output_next(swayc_t *current);
struct sway_container *workspace_next(swayc_t *current);
struct sway_container *workspace_output_prev(swayc_t *current);
struct sway_container *workspace_prev(swayc_t *current);

#endif
