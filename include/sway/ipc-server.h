#ifndef _SWAY_IPC_SERVER_H
#define _SWAY_IPC_SERVER_H
#include <json-c/json.h>
#include <sys/socket.h>
#include "sway/config.h"
#include "sway/tree/container.h"
#include "ipc.h"

struct sway_server;

void ipc_init(struct sway_server *server);

struct sockaddr_un *ipc_user_sockaddr(const char *suffix);

void ipc_event_workspace(struct sway_workspace *old,
		struct sway_workspace *new, const char *change);
void ipc_event_window(struct sway_container *window, const char *change);
void ipc_event_barconfig_update(struct bar_config *bar);
void ipc_event_bar_state_update(struct bar_config *bar);
void ipc_event_mode(const char *mode, bool pango);
void ipc_event_shutdown(const char *reason);
void ipc_event_binding(struct sway_binding *binding);
void ipc_event_tick(const char *payload);

struct ipc_client {
	struct wl_event_source *event_source;
	struct wl_event_source *writable_event_source;
	struct sway_server *server;
	int fd;
	uint32_t security_policy;
	enum ipc_command_type subscribed_events;
	size_t write_buffer_len;
	size_t write_buffer_size;
	char *write_buffer;
	// The following are for storing data between event_loop calls
	uint32_t pending_length;
	enum ipc_command_type pending_type;
	const struct ipc_client_impl *impl;
};

bool ipc_send_reply(struct ipc_client *client, enum ipc_command_type payload_type,
	const char *payload, uint32_t payload_length);

typedef json_object *(*ipc_handler)(struct ipc_client *client,
	enum ipc_command_type *type, char *data);

struct ipc_client_impl {
	size_t num_commands;
	const ipc_handler *commands;
};

extern const struct ipc_client_impl ipc_client_sway;
extern const struct ipc_client_impl ipc_client_i3;

#endif
