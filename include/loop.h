#ifndef _SWAY_LOOP_H
#define _SWAY_LOOP_H
#include <stdbool.h>

/**
 * This is an event loop system designed for sway clients, not sway itself.
 *
 * The loop consists of file descriptors and timers. Typically the Wayland
 * display's file descriptor will be one of the fds in the loop.
 */

struct loop;
struct loop_timer;

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
 * Add a file descriptor to the loop.
 */
void loop_add_fd(struct loop *loop, int fd, short mask,
		void (*func)(int fd, short mask, void *data), void *data);

/**
 * Add a timer to the loop.
 *
 * When the timer expires, the timer will be removed from the loop and freed.
 */
struct loop_timer *loop_add_timer(struct loop *loop, int ms,
		void (*callback)(void *data), void *data);

/**
 * Remove a file descriptor from the loop.
 */
bool loop_remove_fd(struct loop *loop, int fd);

/**
 * Remove a timer from the loop.
 */
bool loop_remove_timer(struct loop *loop, struct loop_timer *timer);

#endif
