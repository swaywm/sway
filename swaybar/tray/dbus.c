#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <dbus/dbus.h>
#include "swaybar/tray/dbus.h"
#include "swaybar/event_loop.h"
#include "log.h"

DBusConnection *conn = NULL;

static void dispatch_watch(int fd, short mask, void *data) {
	sway_log(L_DEBUG, "Dispatching watch");
	DBusWatch *watch = data;

	if (!dbus_watch_get_enabled(watch)) {
		return;
	}

	uint32_t flags = 0;

	if (mask & POLLIN) {
		flags |= DBUS_WATCH_READABLE;
	} if (mask & POLLOUT) {
		flags |= DBUS_WATCH_WRITABLE;
	} if (mask & POLLHUP) {
		flags |= DBUS_WATCH_HANGUP;
	} if (mask & POLLERR) {
		flags |= DBUS_WATCH_ERROR;
	}

	dbus_watch_handle(watch, flags);
}

static dbus_bool_t add_watch(DBusWatch *watch, void *_data) {
	if (!dbus_watch_get_enabled(watch)) {
		// Watch should not be polled
		return TRUE;
	}

	short mask = 0;
	uint32_t flags = dbus_watch_get_flags(watch);

	if (flags & DBUS_WATCH_READABLE) {
		mask |= POLLIN;
	} if (flags & DBUS_WATCH_WRITABLE) {
		mask |= POLLOUT;
	}

	int fd = dbus_watch_get_unix_fd(watch);

	sway_log(L_DEBUG, "Adding DBus watch fd: %d", fd);
	add_event(fd, mask, dispatch_watch, watch);

	return TRUE;
}

static void remove_watch(DBusWatch *watch, void *_data) {
	int fd = dbus_watch_get_unix_fd(watch);

	remove_event(fd);
}

static void dispatch_timeout(timer_t timer, void *data) {
	sway_log(L_DEBUG, "Dispatching DBus timeout");
	DBusTimeout *timeout = data;

	if (dbus_timeout_get_enabled(timeout)) {
		dbus_timeout_handle(timeout);
	}
}

static dbus_bool_t add_timeout(DBusTimeout *timeout, void *_data) {
	if (!dbus_timeout_get_enabled(timeout)) {
		return TRUE;
	}

	timer_t *timer = malloc(sizeof(timer_t));
	if (!timer) {
		sway_log(L_ERROR, "Cannot allocate memory");
		return FALSE;
	}
	struct sigevent ev = {
		.sigev_notify = SIGEV_NONE,
	};

	if (timer_create(CLOCK_MONOTONIC, &ev, timer)) {
		sway_log(L_ERROR, "Could not create DBus timer");
		return FALSE;
	}

	int interval = dbus_timeout_get_interval(timeout);
	int interval_sec = interval / 1000;
	int interval_msec = (interval_sec * 1000) - interval;

	struct timespec period = {
		(time_t) interval_sec,
		((long) interval_msec) * 1000 * 1000,
	};
	struct itimerspec time = {
		period,
		period,
	};

	timer_settime(*timer, 0, &time, NULL);

	dbus_timeout_set_data(timeout, timer, NULL);

	sway_log(L_DEBUG, "Adding DBus timeout. Interval: %ds %dms", interval_sec, interval_msec);
	add_timer(*timer, dispatch_timeout, timeout);

	return TRUE;
}
static void remove_timeout(DBusTimeout *timeout, void *_data) {
	timer_t *timer = (timer_t *) dbus_timeout_get_data(timeout);
	sway_log(L_DEBUG, "Removing DBus timeout.");

	if (timer) {
		remove_timer(*timer);
		timer_delete(*timer);
		free(timer);
	}
}

static bool should_dispatch = true;

static void dispatch_status(DBusConnection *connection, DBusDispatchStatus new_status,
		void *_data) {
	if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
		should_dispatch = true;
	}
}

struct async_prop_data {
	char const *sig;
	void(*callback)(DBusMessageIter *, void *, enum property_status);
	void *usr_data;
};

static void get_prop_callback(DBusPendingCall *pending, void *_data) {
	struct async_prop_data *data = _data;

	DBusMessage *reply = dbus_pending_call_steal_reply(pending);

	if (!reply) {
		sway_log(L_INFO, "Got no icon name reply from item");
		goto bail;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		char *msg;

		dbus_message_get_args(reply, NULL,
				DBUS_TYPE_STRING, &msg,
				DBUS_TYPE_INVALID);

		sway_log(L_INFO, "Failure to get property: %s", msg);
		data->callback(NULL, data->usr_data, PROP_ERROR);
		goto bail;
	}

	DBusMessageIter iter;
	DBusMessageIter variant;

	dbus_message_iter_init(reply, &iter);
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
		sway_log(L_ERROR, "Property relpy type incorrect");
		data->callback(NULL, data->usr_data, PROP_BAD_DATA);
		goto bail;
	}
	dbus_message_iter_recurse(&iter, &variant);

	if (!dbus_message_iter_check_signature(&variant, data->sig)) {
		sway_log(L_INFO, "Property returned has incorrect signatue.");
		data->callback(&variant, data->usr_data, PROP_WRONG_SIG);
		goto bail;
	}

	data->callback(&variant, data->usr_data, PROP_EXISTS);

bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	dbus_pending_call_unref(pending);
}

/* Public functions below -- see header for docs*/

bool dbus_message_iter_check_signature(DBusMessageIter *iter, const char *sig) {
	char *msg_sig = dbus_message_iter_get_signature(iter);
	int result = strcmp(msg_sig, sig);
	dbus_free(msg_sig);
	return (result == 0);
}

bool dbus_get_prop_async(const char *destination,
		const char *path, const char *iface,
		const char *prop, const char *expected_signature,
		void(*callback)(DBusMessageIter *, void *, enum property_status),
		void *usr_data) {
	struct async_prop_data *data = malloc(sizeof(struct async_prop_data));
	if (!data) {
		return false;
	}
	DBusPendingCall *pending;
	DBusMessage *message = dbus_message_new_method_call(
			destination, path,
			"org.freedesktop.DBus.Properties",
			"Get");

	dbus_message_append_args(message,
			DBUS_TYPE_STRING, &iface,
			DBUS_TYPE_STRING, &prop,
			DBUS_TYPE_INVALID);

	bool status =
		dbus_connection_send_with_reply(conn, message, &pending, -1);

	dbus_message_unref(message);

	if (!(pending || status)) {
		sway_log(L_ERROR, "Could not get property");
		return false;
	}

	data->sig = expected_signature;
	data->callback = callback;
	data->usr_data = usr_data;
	dbus_pending_call_set_notify(pending, get_prop_callback, data, free);

	return true;
}

void dispatch_dbus() {
	if (!should_dispatch || !conn) {
		return;
	}

	DBusDispatchStatus status;

	do {
		status = dbus_connection_dispatch(conn);
	} while (status == DBUS_DISPATCH_DATA_REMAINS);

	if (status != DBUS_DISPATCH_COMPLETE) {
		sway_log(L_ERROR, "Cannot dispatch dbus events: %d", status);
	}

	should_dispatch = false;
}

int dbus_init() {
	DBusError error;
	dbus_error_init(&error);

	conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (conn == NULL) {
		sway_log(L_INFO, "Compiled with dbus support, but unable to connect to dbus");
		sway_log(L_INFO, "swaybar will be unable to display tray icons.");
		return -1;
	}

	dbus_connection_set_exit_on_disconnect(conn, FALSE);
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "Cannot get bus connection: %s\n", error.message);
		conn = NULL;
		return -1;
	}

	sway_log(L_INFO, "Unique name: %s\n", dbus_bus_get_unique_name(conn));

	// Will be called if dispatch status changes
	dbus_connection_set_dispatch_status_function(conn, dispatch_status, NULL, NULL);

	if (!dbus_connection_set_watch_functions(conn, add_watch, remove_watch,
			NULL, NULL, NULL)) {
		dbus_connection_set_watch_functions(conn, NULL, NULL, NULL, NULL, NULL);
		sway_log(L_ERROR, "Failed to activate DBUS watch functions");
		return -1;
	}

	if (!dbus_connection_set_timeout_functions(conn, add_timeout, remove_timeout,
			NULL, NULL, NULL)) {
		dbus_connection_set_watch_functions(conn, NULL, NULL, NULL, NULL, NULL);
		dbus_connection_set_timeout_functions(conn, NULL, NULL, NULL, NULL, NULL);
		sway_log(L_ERROR, "Failed to activate DBUS timeout functions");
		return -1;
	}

	return 0;
}
