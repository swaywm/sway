#ifndef _SWAYBAR_DBUS_H
#define _SWAYBAR_DBUS_H

#include <stdbool.h>
#include <dbus/dbus.h>
extern DBusConnection *conn;

/**
 * Should be called in main loop to dispatch events
 */
void dispatch_dbus();

/**
 * Initializes async dbus communication
 */
int dbus_init();

#endif /* _SWAYBAR_DBUS_H */
