#ifndef _SWAYBAR_BAR_H
#define _SWAYBAR_BAR_H

#include "client/registry.h"
#include "client/window.h"
#include "list.h"

struct bar {
	struct config *config;
	struct status_line *status;
	list_t *outputs;
	struct output *focused_output;

	int ipc_event_socketfd;
	int ipc_socketfd;
	int status_read_fd;
	int status_write_fd;
	pid_t status_command_pid;
};

struct output {
	struct window *window;
	struct registry *registry;
	struct output_state *state;
	list_t *workspaces;
#ifdef ENABLE_TRAY
	list_t *items;
#endif
	char *name;
	int idx;
	bool focused;
	bool active;
};

struct workspace {
	int num;
	char *name;
	bool focused;
	bool visible;
	bool urgent;
};

/** Global bar state */
extern struct bar swaybar;

/** True if sway needs to render */
extern bool dirty;

/**
 * Setup bar.
 */
void bar_setup(struct bar *bar, const char *socket_path, const char *bar_id);

/**
 * Create new output struct from name.
 */
struct output *new_output(const char *name);

/**
 * Bar mainloop.
 */
void bar_run(struct bar *bar);

/**
 * free workspace list.
 */
void free_workspaces(list_t *workspaces);

/**
 * Teardown bar.
 */
void bar_teardown(struct bar *bar);

#endif /* _SWAYBAR_BAR_H */
