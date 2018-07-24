#ifndef _SWAY_SCRATCHPAD_H
#define _SWAY_SCRATCHPAD_H

#include "tree/container.h"

/**
 * Move a container to the scratchpad.
 */
void scratchpad_add_container(struct sway_container *con);

/**
 * Remove a container from the scratchpad.
 */
void scratchpad_remove_container(struct sway_container *con);

/**
 * Show or hide the next container on the scratchpad.
 */
void scratchpad_toggle_auto(void);

/**
 * Show or hide a specific container on the scratchpad.
 */
void scratchpad_toggle_container(struct sway_container *con);

#endif
