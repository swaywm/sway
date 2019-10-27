#ifndef _SWAY_SWAY_UTIL_H
#define _SWAY_SWAY_UTIL_H

#include <spawn.h>

/**
 * Close fd and log theoretical case when close(2) failed
 */
void close_warn(int fd);

struct wl_client *spawn_wl_client(char * const cmd[], struct wl_display *display);
struct wl_client *spawn_wl_client_fa(char * const cmd[], struct wl_display *display, posix_spawn_file_actions_t *fa);

#endif//_SWAY_SWAY_UTIL_H
