#define _XOPEN_SOURCE 500

#ifdef __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#else
#include <linux/input-event-codes.h>
#endif
#include <poll.h>
#include <time.h>
#include <wlr/util/log.h>
#include "swaybar/event_loop.h"
#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni_host.h"
#include "swaybar/tray/sni_watcher.h"

static const char *fd_iface = "org.freedesktop.StatusNotifierWatcher";
static const char *fd_notifier_host = "org.freedesktop.StatusNotifierHost";

bool dbus_init() {
	int ret = 0;
	sd_bus *bus = NULL;
	/* BUG: *slot should contained in sni_watcher struct to facilitate multiple
	 * watchers but when done in this way the sd-bus handler callbacks 
	 * (handle_new_icon) were getting bad userdata
	 */

	sd_bus_slot *slot = NULL;
	// TODO: Add other watchers for KDE, etc.
	struct sni_watcher fd_sni_watcher = {0}; 
	
	ret = sd_bus_open_user(&bus);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to connect to user bus: %s",
				strerror(-ret));
		goto error;
	}
	
	wlr_log(WLR_DEBUG, "Connected to user bus");

	fd_sni_watcher.bus = bus;
	fd_sni_watcher.iface = fd_iface;
	sni_watcher_init(&fd_sni_watcher);

	ret = sni_watcher_vtable_init(&fd_sni_watcher, slot);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to init freedesktop watcher vtable: %s",
				strerror(-ret));
		goto error;
	}

	ret = sd_bus_request_name(bus, fd_iface, 0);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to get freedesktop SNI watcher name: %s",
				strerror(-ret));
		goto error;
	}

	char *fd_name = init_dbus_sni_host(fd_notifier_host);
	if (fd_name == NULL) {
		wlr_log(WLR_ERROR, "Failed to init freedesktop Notifier host");
		goto error;
	}
	
	ret = sd_bus_request_name(bus, fd_name, 0);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to get freedesktop notifier host name: %s",
				strerror(-ret));
		goto error;
	}
	
	add_event(sd_bus_get_fd(bus), POLLIN, process_request, bus);

	return true;

error:
	finish_dbus(slot, bus);
	return false;		
}

void finish_dbus(sd_bus_slot *slot, sd_bus *bus) {
	sd_bus_slot_unref(slot);
	sd_bus_unref(bus);
}

void process_request(int fd, short mask, void *data) {
	int ret = 0;
	sd_bus *bus = data;
	wlr_log(WLR_DEBUG, "Processing a bus request");
	while(1) {
		ret = sd_bus_process(bus, NULL);
		
		if (ret == 0) {
			break;
		}
	}
	
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to process bus: %s", strerror(-ret));
		return;
	}

}

int dbus_name_has_owner(sd_bus *bus, const char *name, sd_bus_error *error) {
	sd_bus_message *reply = NULL;
	int ret = 0;
	int has_owner = 0;

	ret = sd_bus_call_method(bus,
			"org.freedesktop.DBus",
			"/org/freedesktop/dbus",
			"org.freedesktop.DBus",
			"NameHasOwner",
			error,
			&reply,
			"s",
			name);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to call NameHasOwner on: %s", name);
		return ret;
	}

	ret = sd_bus_message_read_basic(reply, 'b', &has_owner);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to read NameHasOwner message on: %s", name);
		return sd_bus_error_set_errno(error, ret);
	}

	return has_owner;
}

