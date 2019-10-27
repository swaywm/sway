#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "log.h"
#include "sway/server.h"
#include "sway/swaynag.h"
#include "sway/util.h"
#include "util.h"

static void handle_swaynag_client_destroy(struct wl_listener *listener,
		void *data) {
	struct swaynag_instance *swaynag =
		wl_container_of(listener, swaynag, client_destroy);
	wl_list_remove(&swaynag->client_destroy.link);
	wl_list_init(&swaynag->client_destroy.link);
	swaynag->client = NULL;
}

static struct wl_client *swaynag_spawn_detailed(char *cmd[], struct swaynag_instance *swaynag) {
	if (pipe(swaynag->fd) != 0) {
		sway_log(SWAY_ERROR, "Failed to create pipe for swaynag");
		return NULL;
	}

	if (!set_cloexec(swaynag->fd[1], true)) {
		goto failed;
	}

	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, swaynag->fd[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&fa, swaynag->fd[0]);

	struct wl_client *client = spawn_wl_client_fa(cmd, server.wl_display, &fa);

	posix_spawn_file_actions_destroy(&fa);

	if (client == NULL) {
		goto failed;
	}

	close_warn(swaynag->fd[0]);

	return client;

failed:
	close_warn(swaynag->fd[0]);
	close_warn(swaynag->fd[1]);

	return NULL;
}

bool swaynag_spawn(const char *swaynag_command,
		struct swaynag_instance *swaynag) {
	if (swaynag->client != NULL) {
		wl_client_destroy(swaynag->client);
	}

	if (!swaynag_command) {
		return true;
	}

	size_t length = strlen(swaynag_command) + strlen(swaynag->args) + 2;
	char *swaynag_cmd = malloc(length);
	snprintf(swaynag_cmd, length, "%s %s", swaynag_command, swaynag->args);
	char *cmd[] = {"/bin/sh", "-c", swaynag_cmd, NULL};

	if (swaynag->detailed) {
		swaynag->client = swaynag_spawn_detailed(cmd, swaynag);
	} else {
		swaynag->client = spawn_wl_client(cmd, server.wl_display);
	}

	free(swaynag_cmd);

	if (swaynag->client == NULL) {
		sway_log(SWAY_ERROR, "Failed to spawn process for swaynag");
		return false;
	}

	swaynag->client_destroy.notify = handle_swaynag_client_destroy;
	wl_client_add_destroy_listener(swaynag->client, &swaynag->client_destroy);

	return true;
}

void swaynag_log(const char *swaynag_command, struct swaynag_instance *swaynag,
		const char *fmt, ...) {
	if (!swaynag_command) {
		return;
	}

	if (!swaynag->detailed) {
		sway_log(SWAY_ERROR, "Attempting to write to non-detailed swaynag inst");
		return;
	}

	if (swaynag->client == NULL && !swaynag_spawn(swaynag_command, swaynag)) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	size_t length = vsnprintf(NULL, 0, fmt, args) + 1;
	va_end(args);

	char *temp = malloc(length + 1);
	if (!temp) {
		sway_log(SWAY_ERROR, "Failed to allocate buffer for swaynag log entry.");
		return;
	}

	va_start(args, fmt);
	vsnprintf(temp, length, fmt, args);
	va_end(args);

	write(swaynag->fd[1], temp, length);

	free(temp);
}

void swaynag_show(struct swaynag_instance *swaynag) {
	if (swaynag->detailed && swaynag->client != NULL) {
		close(swaynag->fd[1]);
	}
}

