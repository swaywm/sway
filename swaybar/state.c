#include <stdlib.h>

#include "list.h"
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

void free_workspace(void *item) {
	if (!item) {
		return;
	}
	struct workspace *ws = (struct workspace *)item;
	if (ws->name) {
		free(ws->name);
	}
	free(ws);
}
