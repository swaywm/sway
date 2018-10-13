#ifndef _SWAY_LOOP_H
#define _SWAY_LOOP_H
#include <stdbool.h>

/**
 * This is an event loop system designed for sway clients, not sway itself.
 *
 * It uses pollfds to block on multiple file descriptors at once, and provides
 * an easy way to set timers. Typically the Wayland display's fd will be one of
 * the fds in the loop.
 */

struct loop;

/**
 * Create an event loop.
 */
struct loop *loop_create(void);

/**
 * Destroy the event loop (eg. on program termination).
 */
void loop_destroy(struct loop *loop);

/**
 * Poll the event loop. This will block until one of the fds has data.
 */
void loop_poll(struct loop *loop);

/**
 * Add an fd to the loop.
 */
struct loop_event *loop_add_fd(struct loop *loop, int fd, short mask,
		void (*func)(int fd, short mask, void *data), void *data);

/**
 * Add a timer to the loop.
 */
struct loop_event *loop_add_timer(struct loop *loop, int ms,
		void (*callback)(int fd, short mask, void *data), void *data);

/**
 * Remove an event from the loop.
 */
bool loop_remove_event(struct loop *loop, struct loop_event *event);

#endif
