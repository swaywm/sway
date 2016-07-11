#ifndef _SWAYBAR_IPC_H
#define _SWAYBAR_IPC_H

#include "bar.h"

/**
 * Initialize ipc connection to sway and get sway state, outputs, bar_config.
 */
void ipc_bar_init(struct bar *bar, const char *bar_id);

/**
 * Handle ipc event from sway.
 */
bool handle_ipc_event(struct bar *bar);


/**
 * Send workspace command to sway
 */
void ipc_send_workspace_command(const char *workspace_name);

#endif /* _SWAYBAR_IPC_H */

