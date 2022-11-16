#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/desktop/launcher.h"
#include "sway/tree/node.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"
#include "sway/tree/root.h"
#include "log.h"

static struct wl_list pid_workspaces;

struct pid_workspace {
	pid_t pid;
	char *name;
	struct wlr_xdg_activation_token_v1 *token;
	struct wl_listener token_destroy;

	struct sway_node *node;
	struct wl_listener node_destroy;

	struct wl_list link;
};

/**
 * Get the pid of a parent process given the pid of a child process.
 *
 * Returns the parent pid or NULL if the parent pid cannot be determined.
 */
static pid_t get_parent_pid(pid_t child) {
	pid_t parent = -1;
	char file_name[100];
	char *buffer = NULL;
	const char *sep = " ";
	FILE *stat = NULL;
	size_t buf_size = 0;

	snprintf(file_name, sizeof(file_name), "/proc/%d/stat", child);

	if ((stat = fopen(file_name, "r"))) {
		if (getline(&buffer, &buf_size, stat) != -1) {
			strtok(buffer, sep); // pid
			strtok(NULL, sep);   // executable name
			strtok(NULL, sep);   // state
			char *token = strtok(NULL, sep);   // parent pid
			parent = strtol(token, NULL, 10);
		}
		free(buffer);
		fclose(stat);
	}

	if (parent) {
		return (parent == child) ? -1 : parent;
	}

	return -1;
}

static void pid_workspace_destroy(struct pid_workspace *pw) {
	wl_list_remove(&pw->node_destroy.link);
	wl_list_remove(&pw->token_destroy.link);
	wl_list_remove(&pw->link);
	wlr_xdg_activation_token_v1_destroy(pw->token);
	free(pw->name);
	free(pw);
}

struct sway_workspace *root_workspace_for_pid(pid_t pid) {
	if (!pid_workspaces.prev && !pid_workspaces.next) {
		wl_list_init(&pid_workspaces);
		return NULL;
	}

	struct sway_workspace *ws = NULL;
	struct sway_output *output = NULL;
	struct pid_workspace *pw = NULL;

	sway_log(SWAY_DEBUG, "Looking up workspace for pid %d", pid);

	do {
		struct pid_workspace *_pw = NULL;
		wl_list_for_each(_pw, &pid_workspaces, link) {
			if (pid == _pw->pid) {
				pw = _pw;
				sway_log(SWAY_DEBUG,
					"found %s match for pid %d: %s",
					node_type_to_str(pw->node->type), pid, node_get_name(pw->node));
				break;
			}
		}
		pid = get_parent_pid(pid);
	} while (pid > 1);

	if (pw) {
		switch (pw->node->type) {
		case N_CONTAINER:
			// Unimplemented
			// TODO: add container matching?
			ws = pw->node->sway_container->pending.workspace;
			break;
		case N_WORKSPACE:
			ws = pw->node->sway_workspace;
			break;
		case N_OUTPUT:
			output = pw->node->sway_output;
			ws = workspace_by_name(pw->name);
			if (!ws) {
				sway_log(SWAY_DEBUG,
						"Creating workspace %s for pid %d because it disappeared",
						pw->name, pid);
				if (!output->enabled) {
					sway_log(SWAY_DEBUG,
							"Workspace output %s is disabled, trying another one",
							output->wlr_output->name);
					output = NULL;
				}
				ws = workspace_create(output, pw->name);
			}
			break;
		case N_ROOT:
			ws = workspace_create(NULL, pw->name);
			break;
		}
		pid_workspace_destroy(pw);
	}

	return ws;
}

static void pw_handle_node_destroy(struct wl_listener *listener, void *data) {
	struct pid_workspace *pw = wl_container_of(listener, pw, node_destroy);
	switch (pw->node->type) {
	case N_CONTAINER:
		// Unimplemented
		break;
	case N_WORKSPACE:;
		struct sway_workspace *ws = pw->node->sway_workspace;
		wl_list_remove(&pw->node_destroy.link);
		wl_list_init(&pw->node_destroy.link);
		// We want to save this ws name to recreate later, hopefully on the
		// same output
		free(pw->name);
		pw->name = strdup(ws->name);
		if (!ws->output || ws->output->node.destroying) {
			// If the output is being destroyed it would be pointless to track
			// If the output is being disabled, we'll find out if it's still
			// disabled when we try to match it.
			pw->node = &root->node;
			break;
		}
		pw->node = &ws->output->node;
		wl_signal_add(&pw->node->events.destroy, &pw->node_destroy);
		break;
	case N_OUTPUT:
		wl_list_remove(&pw->node_destroy.link);
		wl_list_init(&pw->node_destroy.link);
		// We'll make the ws pw->name somewhere else
		pw->node = &root->node;
		break;
	case N_ROOT:
		// Unreachable
		break;
	}
}

static void token_handle_destroy(struct wl_listener *listener, void *data) {
	struct pid_workspace *pw = wl_container_of(listener, pw, token_destroy);
	pw->token = NULL;
	pid_workspace_destroy(pw);
}

void root_record_workspace_pid(pid_t pid) {
	sway_log(SWAY_DEBUG, "Recording workspace for process %d", pid);
	if (!pid_workspaces.prev && !pid_workspaces.next) {
		wl_list_init(&pid_workspaces);
	}

	struct sway_seat *seat = input_manager_current_seat();
	struct sway_workspace *ws = seat_get_focused_workspace(seat);
	if (!ws) {
		sway_log(SWAY_DEBUG, "Bailing out, no workspace");
		return;
	}

	struct pid_workspace *pw = calloc(1, sizeof(struct pid_workspace));
	struct wlr_xdg_activation_token_v1 *token =
		wlr_xdg_activation_token_v1_create(server.xdg_activation_v1);
	token->data = pw;
	pw->name = strdup(ws->name);
	pw->token = token;
	pw->node = &ws->node;
	pw->pid = pid;

	pw->node_destroy.notify = pw_handle_node_destroy;
	wl_signal_add(&pw->node->events.destroy, &pw->node_destroy);

	pw->token_destroy.notify = token_handle_destroy;
	wl_signal_add(&token->events.destroy, &pw->token_destroy);

	wl_list_insert(&pid_workspaces, &pw->link);
}

void root_remove_workspace_pid(pid_t pid) {
	if (!pid_workspaces.prev || !pid_workspaces.next) {
		return;
	}

	struct pid_workspace *pw, *tmp;
	wl_list_for_each_safe(pw, tmp, &pid_workspaces, link) {
		if (pid == pw->pid) {
			pid_workspace_destroy(pw);
			return;
		}
	}
}
