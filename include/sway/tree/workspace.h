#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include "sway/tree/container.h"

struct sway_view;

struct sway_workspace {
	struct sway_container *swayc;
	struct sway_view *fullscreen;
};

extern char *prev_workspace_name;

char *workspace_next_name(const char *output_name);

bool workspace_switch(struct sway_container *workspace);

struct sway_container *workspace_by_number(const char* name);

struct sway_container *workspace_by_name(const char*);

struct sway_container *workspace_output_next(struct sway_container *current);

struct sway_container *workspace_next(struct sway_container *current);

struct sway_container *workspace_output_prev(struct sway_container *current);

struct sway_container *workspace_prev(struct sway_container *current);

bool workspace_is_visible(struct sway_container *ws);

#endif
