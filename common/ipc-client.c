#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "ipc-client.h"
#include "log.h"

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

#define IPC_HEADER_SIZE (sizeof(ipc_magic) + 8)

int query_for_swaypid(char *xdg_runtime_dir) {
	if (xdg_runtime_dir != NULL) {
		char pidline[1024];
		char *pid;
		FILE *fp = popen("pidof sway", "r");
		fgets(pidline, 1024, fp);
		pid = strtok(pidline, " ");
		int sway_pid = atoi(pid);
		if (strtok(NULL, " ") != NULL) {
			return -1;
		} else {
			return sway_pid;
		}
	} else {
		exit(EXIT_FAILURE);
	};
}


char *get_socketpath(void) {
	const char *swaysock = getenv("SWAYSOCK");
	if (swaysock) {
		return strdup(swaysock);
	}
	char *line = NULL;
	size_t line_size = 0;
	FILE *fp = popen("sway --get-socketpath 2>/dev/null", "r");
	if (fp) {
		ssize_t nret = getline(&line, &line_size, fp);
		pclose(fp);
		if (nret > 0) {
			// remove trailing newline, if there is one
			if (line[nret - 1] == '\n') {
				line[nret - 1] = '\0';
			}
			return line;
		}
	}
	const char *i3sock = getenv("I3SOCK");
	if (i3sock) {
		free(line);
		return strdup(i3sock);
	}
	fp = popen("i3 --get-socketpath 2>/dev/null", "r");
	if (fp) {
		ssize_t nret = getline(&line, &line_size, fp);
		pclose(fp);
		if (nret > 0) {
			// remove trailing newline, if there is one
			if (line[nret - 1] == '\n') {
				line[nret - 1] = '\0';
			}
			free(line);
			return line;
		}
	}
	free(line);

	if (getenv("XDG_RUNTIME_DIR") && getuid()) {
		int swaypid = query_for_swaypid(getenv("XDG_RUNTIME_DIR"));
		if (swaypid > 0) {
			size_t path_sz = snprintf(NULL, 0, "%s/sway-ipc.%u.%i.sock", getenv("XDG_RUNTIME_DIR"), getuid(), swaypid);
			char *swaysock_runtime = malloc(path_sz+1);
			snprintf(swaysock_runtime, path_sz+1, "%s/sway-ipc.%u.%i.sock", getenv("XDG_RUNTIME_DIR"), getuid(), swaypid);
			if (access(swaysock_runtime, F_OK) != -1) {
				return strdup(swaysock_runtime);
			} else {
				fprintf(stderr, "sway socket not detected.\n"); // found a pid, but no socket file
			}
		} else {
			fprintf(stderr, "Found more than one sway instance running.\n");
			exit(EXIT_FAILURE);
		}
	}
	return NULL;
}

int ipc_open_socket(const char *socket_path) {
	struct sockaddr_un addr;
	int socketfd;
	if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		sway_abort("Unable to open Unix socket");
	}
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
	addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
	int l = sizeof(struct sockaddr_un);
	if (connect(socketfd, (struct sockaddr *)&addr, l) == -1) {
		sway_abort("Unable to connect to %s", socket_path);
	}
	return socketfd;
}

bool ipc_set_recv_timeout(int socketfd, struct timeval tv) {
	if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
		sway_log_errno(SWAY_ERROR, "Failed to set ipc recv timeout");
		return false;
	}
	return true;
}

struct ipc_response *ipc_recv_response(int socketfd) {
	char data[IPC_HEADER_SIZE];

	size_t total = 0;
	while (total < IPC_HEADER_SIZE) {
		ssize_t received = recv(socketfd, data + total, IPC_HEADER_SIZE - total, 0);
		if (received <= 0) {
			sway_abort("Unable to receive IPC response");
		}
		total += received;
	}

	struct ipc_response *response = malloc(sizeof(struct ipc_response));
	if (!response) {
		goto error_1;
	}

	memcpy(&response->size, data + sizeof(ipc_magic), sizeof(uint32_t));
	memcpy(&response->type, data + sizeof(ipc_magic) + sizeof(uint32_t), sizeof(uint32_t));

	char *payload = malloc(response->size + 1);
	if (!payload) {
		goto error_2;
	}

	total = 0;
	while (total < response->size) {
		ssize_t received = recv(socketfd, payload + total, response->size - total, 0);
		if (received < 0) {
			sway_abort("Unable to receive IPC response");
		}
		total += received;
	}
	payload[response->size] = '\0';
	response->payload = payload;

	return response;
error_2:
	free(response);
error_1:
	sway_log(SWAY_ERROR, "Unable to allocate memory for IPC response");
	return NULL;
}

void free_ipc_response(struct ipc_response *response) {
	free(response->payload);
	free(response);
}

char *ipc_single_command(int socketfd, uint32_t type, const char *payload, uint32_t *len) {
	char data[IPC_HEADER_SIZE];
	memcpy(data, ipc_magic, sizeof(ipc_magic));
	memcpy(data + sizeof(ipc_magic), len, sizeof(*len));
	memcpy(data + sizeof(ipc_magic) + sizeof(*len), &type, sizeof(type));

	if (write(socketfd, data, IPC_HEADER_SIZE) == -1) {
		sway_abort("Unable to send IPC header");
	}

	if (write(socketfd, payload, *len) == -1) {
		sway_abort("Unable to send IPC payload");
	}

	struct ipc_response *resp = ipc_recv_response(socketfd);
	char *response = resp->payload;
	*len = resp->size;
	free(resp);

	return response;
}
