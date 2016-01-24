#ifndef _SWAYBAR_STATE_H
#define _SWAYBAR_STATE_H

#include "client/registry.h"
#include "client/window.h"
#include "list.h"

struct swaybar_state {
	struct swaybar_config *config;
	struct status_line *status;
	struct output *output;
	/* list_t *outputs; */

	int ipc_event_socketfd;
	int ipc_socketfd;
	int status_read_fd;
	pid_t status_command_pid;
};

struct output {
	struct window *window;
	struct registry *registry;
	list_t *workspaces;
	char *name;
};

struct workspace {
	int num;
	char *name;
	bool focused;
	bool visible;
	bool urgent;
};

/**
 * Initialize state.
 */
struct swaybar_state *init_state();

/**
 * free workspace list.
 */
void free_workspaces(list_t *workspaces);

/**
 * Free state struct.
 */
void free_state(struct swaybar_state *state);

#endif /* _SWAYBAR_STATE_H */
