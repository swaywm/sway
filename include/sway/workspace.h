#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

struct sway_container;

extern char *prev_workspace_name;

char *workspace_next_name(const char *output_name);
swayc_t *workspace_create(const char *name);
bool workspace_switch(swayc_t *workspace);

struct sway_container *workspace_by_number(const char* name);
swayc_t *workspace_by_name(const char*);

struct sway_container *workspace_output_next(struct sway_container *current);
struct sway_container *workspace_next(struct sway_container *current);
struct sway_container *workspace_output_prev(struct sway_container *current);
struct sway_container *workspace_prev(struct sway_container *current);

#endif
