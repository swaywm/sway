#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <wlc/wlc.h>
#include <unistd.h>
#include "log.h"
#include "config.h"
#include "commands.h"

static int ipc_socket = -1;

int ipc_handle_connection(int fd, uint32_t mask, void *data);

void init_ipc() {
	ipc_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ipc_socket == -1) {
		sway_abort("Unable to create IPC socket");
	}

	struct sockaddr_un ipc_sockaddr = {
		.sun_family = AF_UNIX,
		// TODO: use a proper socket path
		.sun_path = "/tmp/sway.sock"
	};

	unlink(ipc_sockaddr.sun_path);
	if (bind(ipc_socket, (struct sockaddr *)&ipc_sockaddr, sizeof(ipc_sockaddr)) == -1) {
		sway_abort("Unable to bind IPC socket");
	}

	if (listen(ipc_socket, 3) == -1) {
		sway_abort("Unable to listen on IPC socket");
	}

	wlc_event_loop_add_fd(ipc_socket, WLC_EVENT_READABLE, ipc_handle_connection, NULL);
}

int ipc_handle_connection(int /*fd*/, uint32_t /*mask*/, void */*data*/) {
	int client_socket = accept(ipc_socket, NULL, NULL);
	if (client_socket == -1) {
		char error[256];
		strerror_r(errno, error, sizeof(error));
		sway_log(L_INFO, "Unable to accept IPC client connection: %s", error);
		return 0;
	}

	char buf[1024];
	if (recv(client_socket, buf, sizeof(buf), 0) == -1) {
		char error[256];
		strerror_r(errno, error, sizeof(error));
		sway_log(L_INFO, "Unable to receive from IPC client: %s", error);
		close(client_socket);
		return 0;
	}

	sway_log(L_INFO, "Executing IPC command: %s", buf);

	bool success = handle_command(config, buf);
	snprintf(buf, sizeof(buf), "{\"success\":%s}\n", success ? "true" : "false");

	if (send(client_socket, buf, strlen(buf), 0) == -1) {
		char error[256];
		strerror_r(errno, error, sizeof(error));
		sway_log(L_INFO, "Unable to send to IPC client: %s", error);
	}

	close(client_socket);
	return 0;
}
