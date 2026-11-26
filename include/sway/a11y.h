#ifndef _SWAY_A11Y_H
#define _SWAY_A11Y_H
#include <stdbool.h>
#include <wayland-server-core.h>

struct sway_keyboard;
struct wlr_keyboard_key_event;

/**
 * The org.freedesktop.a11y.KeyboardMonitor D-Bus service, used by screen
 * readers such as Orca to watch and grab keyboard input.
 *
 * Only built when HAVE_A11Y is set.
 */

/**
 * Connect to the session bus, export the interface and request the
 * org.freedesktop.a11y.Manager name. Without a session bus this only logs a
 * message, and the key hook keeps passing every key through.
 */
void a11y_init(struct wl_event_loop *loop);

void a11y_finish(void);

/**
 * Report a key event to the interested clients and decide whether it is
 * grabbed. Returns true if the key must not be handled any further: it must
 * neither reach a client nor run a binding.
 *
 * With keyboard grouping, the same event passes through here once for the
 * member keyboard and once for the group keyboard; it is processed once and
 * both passes get the same answer.
 */
bool a11y_handle_key(struct sway_keyboard *keyboard,
		struct wlr_keyboard_key_event *event);

#endif
