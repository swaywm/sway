#ifndef _SWAYBAR_DBUS_H
#define _SWAYBAR_DBUS_H

#include <stdbool.h>
#include <dbus/dbus.h>
extern DBusConnection *conn;

/**
 * Checks the signature of the given iter against `sig`. Prefer to
 * `dbus_message_iter_get_signature` as this one frees the intermediate string.
 */
bool dbus_message_iter_check_signature(DBusMessageIter *iter, const char *sig);

/**
 * Should be called in main loop to dispatch events
 */
void dispatch_dbus();

/**
 * Initializes async dbus communication
 */
int dbus_init();

#endif /* _SWAYBAR_DBUS_H */
