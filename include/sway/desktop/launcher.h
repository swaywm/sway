#ifndef _SWAY_LAUNCHER_H
#define _SWAY_LAUNCHER_H

#include <stdlib.h>

struct launcher_ctx {
	pid_t pid;
	char *name;
	struct wlr_xdg_activation_token_v1 *token;
	struct wl_listener token_destroy;

	bool activated;

	struct sway_node *node;
	struct wl_listener node_destroy;

	struct wl_list link; // sway_server::pending_launcher_ctxs
};

struct launcher_ctx *launcher_ctx_find_pid(pid_t pid);

struct sway_workspace *launcher_ctx_get_workspace(struct launcher_ctx *ctx);

void launcher_ctx_consume(struct launcher_ctx *ctx);

void launcher_ctx_destroy(struct launcher_ctx *ctx);

struct launcher_ctx *launcher_ctx_create(void);

const char *launcher_ctx_get_token_name(struct launcher_ctx *ctx);

#endif
