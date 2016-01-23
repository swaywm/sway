#ifndef _SWAYBAR_IPC_H
#define _SWAYBAR_IPC_H

#include "state.h"

/**
 * Initialize ipc connection to sway and get sway state, outputs, bar_config.
 */
void ipc_bar_init(struct swaybar_state *state, int outputi, const char *bar_id);

/**
 * Handle ipc event from sway.
 */
bool handle_ipc_event(struct swaybar_state *state);

#endif /* _SWAYBAR_IPC_H */

