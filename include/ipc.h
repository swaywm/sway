#ifndef _SWAY_IPC_H
#define _SWAY_IPC_H

enum ipc_command_type {
	IPC_COMMAND = 0,
	IPC_GET_WORKSPACES = 1,
	IPC_SUBSCRIBE = 2,
	IPC_GET_OUTPUTS = 3,
	IPC_GET_TREE = 4,
	IPC_GET_MARKS = 5,
	IPC_GET_BAR_CONFIG = 6,
	IPC_GET_VERSION = 7,
	IPC_GET_INPUTS = 8,
	// Events send from sway to clients. Events have the higest bits set.
	IPC_EVENT_WORKSPACE = (1 << (31 - 0)),
	IPC_EVENT_OUTPUT = (1 << (31 - 1)),
	IPC_EVENT_MODE = (1 << (31 - 2)),
	IPC_EVENT_WINDOW = (1 << (32 - 3)),
	IPC_EVENT_BARCONFIG_UPDATE = (1 << (31 - 4)),
	IPC_EVENT_BINDING = (1 << (31 - 5)),
	IPC_EVENT_MODIFIER = (1 << (31 - 6)),
	IPC_EVENT_INPUT = (1 << (31 - 7)),
	IPC_SWAY_GET_PIXELS = 0x81
};

#endif
