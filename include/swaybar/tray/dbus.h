#ifndef _SWAYBAR_DBUS_H
#define _SWAYBAR_DBUS_H

#include <stdbool.h>
#include <dbus/dbus.h>
extern DBusConnection *conn;

enum property_status {
	PROP_EXISTS, /* Will give iter */
	PROP_ERROR, /* Will not give iter */
	PROP_BAD_DATA, /* Will not give iter */
	PROP_WRONG_SIG, /* Will give iter, please be careful */
};

/**
 * Checks the signature of the given iter against `sig`. Prefer to
 * `dbus_message_iter_get_signature` as this one frees the intermediate string.
 */
bool dbus_message_iter_check_signature(DBusMessageIter *iter, const char *sig);

/**
 * Fetches the property and calls `callback` with a message iter pointing it.
 * Performs error handling and signature checking.
 *
 * Returns: true if message is successfully sent and false otherwise. If there
 * is an error getting a property, `callback` will still be run, but with
 * `status` set to the error.
 *
 * NOTE: `expected_signature` must remain valid until the message reply is
 * received, please only use 'static signatures.
 */
bool dbus_get_prop_async(const char *destination,
		const char *path,
		const char *iface,
		const char *prop,
		const char *expected_signature,
		void(*callback)(DBusMessageIter *iter, void *data, enum property_status status),
		void *data);
/**
 * Should be called in main loop to dispatch events
 */
void dispatch_dbus();

/**
 * Initializes async dbus communication
 */
int dbus_init();

#endif /* _SWAYBAR_DBUS_H */
