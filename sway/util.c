#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server.h>

#include "log.h"
#include "sway/util.h"
#include "util.h"

extern char **environ;

void close_warn(int fd) {
	if (close(fd) != 0) {
		sway_log_errno(SWAY_ERROR, "close failed");
	}
}

struct wl_client *spawn_wl_client(char * const cmd[], struct wl_display *display) {
	return spawn_wl_client_fa(cmd, display, NULL);
}

struct wl_client *spawn_wl_client_fa(char * const cmd[], struct wl_display *display, posix_spawn_file_actions_t *fa) {
	int sockets[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
		sway_log_errno(SWAY_ERROR, "socketpair failed");
		return NULL;
	}

	if (!set_cloexec(sockets[0], true) || !set_cloexec(sockets[1], true)) {
		goto cleanup_sockets;
	}

	struct wl_client *client = wl_client_create(display, sockets[0]);
	if (client == NULL) {
		sway_log_errno(SWAY_ERROR, "wl_client_create failed");
		goto cleanup_sockets;
	}

	pid_t pid = fork();
	if (pid < 0) {
		sway_log(SWAY_ERROR, "Failed to create fork for swaybar");
		goto cleanup_client;
	} else if (pid == 0) {
		posix_spawnattr_t attr;
		posix_spawnattr_init(&attr);

		// Remove the SIGUSR1 handler that wlroots adds for xwayland
		sigset_t set;
		sigfillset(&set);
		posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF);
		posix_spawnattr_setsigdefault(&attr, &set);

		char wayland_socket_str[16];
		snprintf(wayland_socket_str, sizeof(wayland_socket_str),
				"%d", sockets[1]);
		setenv("WAYLAND_SOCKET", wayland_socket_str, true);

		if (!set_cloexec(sockets[1], false)) {
			_exit(EXIT_FAILURE);
		}

		int r = posix_spawnp(&pid, cmd[0], fa, &attr, cmd, environ);
		if (r) {
			sway_log_errno(SWAY_ERROR, "posix_spawnp failed");
			_exit(EXIT_FAILURE);
		}

		posix_spawnattr_destroy(&attr);

		_exit(EXIT_SUCCESS);
	}

	if (waitpid(pid, NULL, 0) < 0) {
		sway_log_errno(SWAY_ERROR, "waitpid failed");
		goto cleanup_client;
	}

	close_warn(sockets[1]);

	return client;

cleanup_client:
	wl_client_destroy(client);

cleanup_sockets:
	close(sockets[0]);
	close(sockets[1]);

	return NULL;
}
