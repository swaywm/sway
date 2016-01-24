#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "list.h"
#include "log.h"
#include "config.h"
#include "status_line.h"
#include "state.h"

struct swaybar_state *init_state() {
	struct swaybar_state *state = calloc(1, sizeof(struct swaybar_state));
	state->config = init_config();
	state->status = init_status_line();
	state->output = malloc(sizeof(struct output));
	state->output->window = NULL;
	state->output->registry = NULL;
	state->output->workspaces = create_list();
	state->output->name = NULL;

	return state;
}

void free_workspaces(list_t *workspaces) {
	int i;
	for (i = 0; i < workspaces->length; ++i) {
		struct workspace *ws = workspaces->items[i];
		free(ws->name);
		free(ws);
	}
	list_free(workspaces);
}

static void free_output(struct output *output) {
	window_teardown(output->window);
	if (output->registry) {
		registry_teardown(output->registry);
	}

	free(output->name);

	if (output->workspaces) {
		free_workspaces(output->workspaces);
	}

	free(output);
}

static void terminate_status_command(pid_t pid) {
	if (pid) {
		// terminate status_command process
		int ret = kill(pid, SIGTERM);
		if (ret != 0) {
			sway_log(L_ERROR, "Unable to terminate status_command [pid: %d]", pid);
		} else {
			int status;
			waitpid(pid, &status, 0);
		}
	}
}

void free_state(struct swaybar_state *state) {
	free_config(state->config);
	free_output(state->output);
	free_status_line(state->status);

	/* close sockets/pipes */
	if (state->status_read_fd) {
		close(state->status_read_fd);
	}

	if (state->ipc_socketfd) {
		close(state->ipc_socketfd);
	}

	if (state->ipc_event_socketfd) {
		close(state->ipc_event_socketfd);
	}

	/* terminate status command process */
	terminate_status_command(state->status_command_pid);

	free(state);
}
