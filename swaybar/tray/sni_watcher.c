#include <stddef.h>
#include <stdlib.h>

#include <wlr/util/log.h>

#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni_watcher.h"

static const char *sni_watcher_path = "/StatusNotifierWatcher";

static int method_register_sni(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error) {
	struct sni_watcher *watcher = userdata;
	int ret = 0;
	char *name;
	
	wlr_log(WLR_INFO, "Interface: %s", watcher->iface);	
	ret = sd_bus_message_read(msg, "s", &name);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to parse register SNI item message: %s",
				strerror(-ret));
		return ret;
	}
	
	wlr_log(WLR_DEBUG, "Incoming SNI registration: %s", name);

	if (!dbus_name_has_owner(sd_bus_message_get_bus(msg), name, NULL)) {
		wlr_log(WLR_DEBUG, "DBus name does not have owner");
		return ret;
	}

	wlr_log(WLR_INFO, "RegisterStatusNotifierItem called with: %s", name);
	if (list_seq_find(watcher->items, sni_item_cmp, name) != -1) {
		wlr_log(WLR_DEBUG, "Watcher already has name: %s", name);
	} else {
		list_add(watcher->items, name);
		sd_bus_emit_signal(watcher->bus, watcher->path, watcher->iface,
			"StatusNotifierItemRegistered", "b", "1");
	}

	return sd_bus_reply_method_return(msg, "");
}

static int property_registered_sni(sd_bus *bus, const char *path, const char 
		*interface, const char *property, sd_bus_message *reply, 
		void *userdata, sd_bus_error *error) {
	struct sni_watcher *watcher = userdata;
	int ret = 0;

	ret = sd_bus_message_open_container(reply, 'a', "s");
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to open reply container: %s", 
					strerror(-ret));
		return ret;
	}
	for (int i = 0;i < watcher->items->length; i++) {
		ret = sd_bus_message_append(reply, "s", watcher->items->items[i]);
		if (ret < 0) {
			wlr_log(WLR_ERROR, "Failed to append to reply container: %s",
					strerror(-ret));
			return ret;
		}
	}

	return sd_bus_message_close_container(reply);
}

static const sd_bus_vtable sni_watcher_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("RegisterStatusNotifierItem", "s", "", method_register_sni,
		SD_BUS_VTABLE_UNPRIVILEGED),
//	SD_BUS_METHOD("RegisterStatusNotifierHost", "s", "", method_register_snh,
//		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_PROPERTY("RegisteredStatusNotifierItems", "as", property_registered_sni, 
			0, SD_BUS_VTABLE_PROPERTY_CONST),
//	SD_BUS_PROPERTY("IsStatusNotifierHostRegistered", "b", property_is_snh_registered, 
//			0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("ProtocolVersion", "i", NULL, offsetof(struct sni_watcher, version), 
			SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_SIGNAL("StatusNotifierItemRegistered", "s", 0),
	SD_BUS_SIGNAL("StatusNotifierItemUnregistered", "s", 0),
	SD_BUS_SIGNAL("StatusNotifierHostRegistered", NULL, 0),
	SD_BUS_VTABLE_END
};

bool sni_watcher_vtable_init(struct sni_watcher *watcher, sd_bus_slot *slot) {
	int ret = 0;

	ret = sd_bus_add_object_vtable(watcher->bus, &slot, watcher->path, watcher->iface,
			sni_watcher_vtable, watcher);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Could not init SNI watcher vtable: %s", 
				strerror(-1));
		return false;
	}

	return true;
}

bool sni_watcher_init(struct sni_watcher *watcher) {
	int ret = 0;

	watcher->path = sni_watcher_path;
	watcher->items = create_list();
	watcher->hosts = create_list();
	watcher->version = 0;
	ret = add_sni_signal_matches(watcher);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Could not init SNI watcher matches: %s", 
				strerror(-1));
		return false;
	}
	return true;
}

static int handle_new_icon(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error) {
	struct sni_watcher *watcher = userdata;
	int ret = 0;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	char *name;
	
	ret = sd_bus_get_property_string(watcher->bus, sd_bus_message_get_sender(msg),
		"/StatusNotifierItem", sd_bus_message_get_interface(msg), "IconName",
		&err,
		&name);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to get IconName property: %s", 
				err.message);
		goto finish;	
	}

	wlr_log(WLR_DEBUG, "Got IconName property: %s", name);
	// TODO: call code to find the icon path, display the icon, etc.

finish:
	sd_bus_error_free(&err);
	return ret;
}

int add_sni_signal_matches(struct sni_watcher *watcher) {
	int ret = 0;
	char *iface = NULL;

	if (strcmp(watcher->iface, "org.freedesktop.StatusNotifierWatcher") == 0) {
		iface = "org.freedesktop.StatusNotifierItem";
	}
	
	if (iface == NULL) {
		wlr_log(WLR_ERROR, "Unsupported interface: %s", watcher->iface);
		return -1;
	}
	
	ret = sd_bus_match_signal(watcher->bus, NULL, NULL, NULL, iface, "NewIcon",
			handle_new_icon, watcher);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to add match for NewIcon signal: %s", 
				strerror(-ret));
		return ret;
	}
	wlr_log(WLR_DEBUG, "Added NewIcon signal match for: %s", iface);

	return ret;
}

int sni_item_cmp(const void *item, const void *data) {
	const char *sni_item = item;
	const char *name = data;
	return strcmp(sni_item, name);
}

