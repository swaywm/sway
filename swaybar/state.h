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
 * Setup state.
 */
void state_setup(struct swaybar_state *state, const char *socket_path, const char *bar_id, int desired_output);

/**
 * State mainloop.
 */
void state_run(struct swaybar_state *state);

/**
 * free workspace list.
 */
void free_workspaces(list_t *workspaces);

/**
 * Teardown state.
 */
void state_teardown(struct swaybar_state *state);

#endif /* _SWAYBAR_STATE_H */
