#ifndef _SWAY_WORKSPACE_H
#define _SWAY_WORKSPACE_H

#include "sway/tree/container.h"

struct sway_view;

struct sway_workspace {
	struct sway_container *swayc;
	struct sway_view *fullscreen;
	struct sway_container *floating;
	list_t *output_priority;
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

bool workspace_is_empty(struct sway_container *ws);

void workspace_output_raise_priority(struct sway_container *workspace,
		struct sway_container *old_output, struct sway_container *new_output);

void workspace_output_add_priority(struct sway_container *workspace,
		struct sway_container *output);

struct sway_container *workspace_output_get_highest_available(
		struct sway_container *ws, struct sway_container *exclude);

struct sway_container *workspace_for_pid(pid_t pid);

void workspace_record_pid(pid_t pid);

#endif
