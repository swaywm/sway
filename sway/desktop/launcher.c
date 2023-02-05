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

void launcher_ctx_consume(struct launcher_ctx *ctx) {
	// The view is now responsible for destroying this ctx
	wl_list_remove(&ctx->token_destroy.link);
	wl_list_init(&ctx->token_destroy.link);

	if (!ctx->activated) {
		// An unactivated token hasn't been destroyed yet
		wlr_xdg_activation_token_v1_destroy(ctx->token);
	}
	ctx->token = NULL;

	// Prevent additional matches
	wl_list_remove(&ctx->link);
	wl_list_init(&ctx->link);
}

void launcher_ctx_destroy(struct launcher_ctx *ctx) {
	if (ctx == NULL) {
		return;
	}
	wl_list_remove(&ctx->node_destroy.link);
	wl_list_remove(&ctx->token_destroy.link);
	wl_list_remove(&ctx->link);
	wlr_xdg_activation_token_v1_destroy(ctx->token);
	free(ctx->fallback_name);
	free(ctx);
}

struct launcher_ctx *launcher_ctx_find_pid(pid_t pid) {
	if (wl_list_empty(&server.pending_launcher_ctxs)) {
		return NULL;
	}

	struct launcher_ctx *ctx = NULL;
	sway_log(SWAY_DEBUG, "Looking up workspace for pid %d", pid);

	do {
		struct launcher_ctx *_ctx = NULL;
		wl_list_for_each(_ctx, &server.pending_launcher_ctxs, link) {
			if (pid == _ctx->pid) {
				ctx = _ctx;
				sway_log(SWAY_DEBUG,
					"found %s match for pid %d: %s",
					node_type_to_str(ctx->node->type), pid, node_get_name(ctx->node));
				break;
			}
		}
		pid = get_parent_pid(pid);
	} while (pid > 1);

	return ctx;
}

struct sway_workspace *launcher_ctx_get_workspace(
		struct launcher_ctx *ctx) {
	struct sway_workspace *ws = NULL;
	struct sway_output *output = NULL;

	switch (ctx->node->type) {
	case N_CONTAINER:
		// Unimplemented
		// TODO: add container matching?
		ws = ctx->node->sway_container->pending.workspace;
		break;
	case N_WORKSPACE:
		ws = ctx->node->sway_workspace;
		break;
	case N_OUTPUT:
		output = ctx->node->sway_output;
		ws = workspace_by_name(ctx->fallback_name);
		if (!ws) {
			sway_log(SWAY_DEBUG,
					"Creating workspace %s for pid %d because it disappeared",
					ctx->fallback_name, ctx->pid);
			if (!output->enabled) {
				sway_log(SWAY_DEBUG,
						"Workspace output %s is disabled, trying another one",
						output->wlr_output->name);
				output = NULL;
			}
			ws = workspace_create(output, ctx->fallback_name);
		}
		break;
	case N_ROOT:
		ws = workspace_create(NULL, ctx->fallback_name);
		break;
	}

	return ws;
}

static void ctx_handle_node_destroy(struct wl_listener *listener, void *data) {
	struct launcher_ctx *ctx = wl_container_of(listener, ctx, node_destroy);
	switch (ctx->node->type) {
	case N_CONTAINER:
		// Unimplemented
		break;
	case N_WORKSPACE:;
		struct sway_workspace *ws = ctx->node->sway_workspace;
		wl_list_remove(&ctx->node_destroy.link);
		wl_list_init(&ctx->node_destroy.link);
		// We want to save this ws name to recreate later, hopefully on the
		// same output
		free(ctx->fallback_name);
		ctx->fallback_name = strdup(ws->name);
		if (!ws->output || ws->output->node.destroying) {
			// If the output is being destroyed it would be pointless to track
			// If the output is being disabled, we'll find out if it's still
			// disabled when we try to match it.
			ctx->node = &root->node;
			break;
		}
		ctx->node = &ws->output->node;
		wl_signal_add(&ctx->node->events.destroy, &ctx->node_destroy);
		break;
	case N_OUTPUT:
		wl_list_remove(&ctx->node_destroy.link);
		wl_list_init(&ctx->node_destroy.link);
		// We'll make the ws ctx->name somewhere else
		ctx->node = &root->node;
		break;
	case N_ROOT:
		// Unreachable
		break;
	}
}

static void token_handle_destroy(struct wl_listener *listener, void *data) {
	struct launcher_ctx *ctx = wl_container_of(listener, ctx, token_destroy);
	ctx->token = NULL;
	launcher_ctx_destroy(ctx);
}

struct launcher_ctx *launcher_ctx_create(struct wlr_xdg_activation_token_v1 *token,
		struct sway_node *node) {
	struct launcher_ctx *ctx = calloc(1, sizeof(struct launcher_ctx));

	const char *fallback_name = NULL;
	struct sway_workspace *ws = NULL;
	switch (node->type) {
	case N_CONTAINER:
		// Unimplemented
		free(ctx);
		return NULL;
	case N_WORKSPACE:
		ws = node->sway_workspace;
		fallback_name = ws->name;
		break;
	case N_OUTPUT:;
		struct sway_output *output = node->sway_output;
		ws = output_get_active_workspace(output);
		fallback_name = ws ? ws->name : NULL;
		break;
	case N_ROOT:
		// Unimplemented
		free(ctx);
		return NULL;
	}

	if (!fallback_name) {
		// TODO: implement a better fallback.
		free(ctx);
		return NULL;
	}

	ctx->fallback_name = strdup(fallback_name);
	ctx->token = token;
	ctx->node = node;

	ctx->node_destroy.notify = ctx_handle_node_destroy;
	wl_signal_add(&ctx->node->events.destroy, &ctx->node_destroy);

	ctx->token_destroy.notify = token_handle_destroy;
	wl_signal_add(&token->events.destroy, &ctx->token_destroy);

	wl_list_init(&ctx->link);
	wl_list_insert(&server.pending_launcher_ctxs, &ctx->link);

	token->data = ctx;
	return ctx;
}

// Creates a context with a new token for the internal launcher
struct launcher_ctx *launcher_ctx_create_internal() {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_workspace *ws = seat_get_focused_workspace(seat);
	if (!ws) {
		sway_log(SWAY_DEBUG, "Failed to create launch context. No workspace.");
		return NULL;
	}

	struct wlr_xdg_activation_token_v1 *token =
		wlr_xdg_activation_token_v1_create(server.xdg_activation_v1);
	token->seat = seat->wlr_seat;

	struct launcher_ctx *ctx = launcher_ctx_create(token, &ws->node);
	if (!ctx) {
		wlr_xdg_activation_token_v1_destroy(token);
		return NULL;
	}

	return ctx;
}

const char *launcher_ctx_get_token_name(struct launcher_ctx *ctx) {
	const char *token = wlr_xdg_activation_token_v1_get_name(ctx->token);
	return token;
}
