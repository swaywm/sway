#ifndef _SWAY_IPC_H
#define _SWAY_IPC_H

enum ipc_command_type {
	IPC_COMMAND = 0,
	IPC_GET_WORKSPACES = 1,
	IPC_SUBSCRIBE = 2,
	IPC_GET_OUTPUTS	= 3,
	IPC_GET_TREE = 4,
	IPC_GET_MARKS = 5,
	IPC_GET_BAR_CONFIG = 6,
	IPC_GET_VERSION	= 7,
};

void ipc_init(void);
void ipc_terminate(void);

#endif
