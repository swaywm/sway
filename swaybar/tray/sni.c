#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dbus/dbus.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni.h"
#include "swaybar/tray/icon.h"
#include "swaybar/bar.h"
#include "cairo.h"
#include "log.h"

static const char *KDE_IFACE = "org.kde.StatusNotifierItem";
static const char *FD_IFACE = "org.freedesktop.StatusNotifierItem";

// Not sure what this is but cairo needs it.
static const cairo_user_data_key_t cairo_user_data_key;

struct sni_icon_ref *sni_icon_ref_create(struct StatusNotifierItem *item,
		int height) {
	struct sni_icon_ref *sni_ref = malloc(sizeof(struct sni_icon_ref));
	if (!sni_ref) {
		return NULL;
	}
	sni_ref->icon = cairo_image_surface_scale(item->image, height, height);
	sni_ref->ref = item;

	return sni_ref;
}

void sni_icon_ref_free(struct sni_icon_ref *sni_ref) {
	if (!sni_ref) {
		return;
	}
	cairo_surface_destroy(sni_ref->icon);
	free(sni_ref);
}

/* Gets the pixmap of an icon */
static void reply_icon(DBusMessageIter *iter /* a(iiay) */, void *_data,
		enum property_status status) {
	if (status != PROP_EXISTS) {
		return;
	}
	struct StatusNotifierItem *item = _data;

	DBusMessageIter d_struct; /* (iiay) */
	DBusMessageIter struct_items;
	DBusMessageIter icon;

	if (dbus_message_iter_get_element_count(iter) == 0) {
		// Can't recurse if there are no items
		wlr_log(L_INFO, "Item has no icon");
		return;
	}

	dbus_message_iter_recurse(iter, &d_struct);
	dbus_message_iter_recurse(&d_struct, &struct_items);

	int width;
	dbus_message_iter_get_basic(&struct_items, &width);
	dbus_message_iter_next(&struct_items);

	int height;
	dbus_message_iter_get_basic(&struct_items, &height);
	dbus_message_iter_next(&struct_items);

	int len = dbus_message_iter_get_element_count(&struct_items);

	if (!len) {
		wlr_log(L_ERROR, "No icon data");
		return;
	}

	// Also implies len % 4 == 0, useful below
	if (len != width * height * 4) {
		wlr_log(L_ERROR, "Incorrect array size passed");
		return;
	}

	dbus_message_iter_recurse(&struct_items, &icon);

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	// FIXME support a variable stride
	// (works on my machine though for all tested widths)
	if (!sway_assert(stride == width * 4, "Stride must be equal to byte length")) {
		return;
	}

	// Data is by reference, no need to free
	uint8_t *message_data;
	dbus_message_iter_get_fixed_array(&icon, &message_data, &len);

	uint8_t *image_data = malloc(stride * height);
	if (!image_data) {
		wlr_log(L_ERROR, "Could not allocate memory for icon");
		return;
	}

	// Transform from network byte order to host byte order
	// Assumptions are safe because the equality above
	uint32_t *network = (uint32_t *) message_data;
	uint32_t *host = (uint32_t *)image_data;
	for (int i = 0; i < width * height; ++i) {
		host[i] = ntohl(network[i]);
	}

	cairo_surface_t *image = cairo_image_surface_create_for_data(
			image_data, CAIRO_FORMAT_ARGB32,
			width, height, stride);

	if (image) {
		if (item->image) {
			cairo_surface_destroy(item->image);
		}
		item->image = image;
		// Free the image data on surface destruction
		cairo_surface_set_user_data(image,
				&cairo_user_data_key,
				image_data,
				free);
		item->dirty = true;
		// TODO TRAY rerender
		//dirty = true;

		return;
	} else {
		wlr_log(L_ERROR, "Could not create image surface");
		free(image_data);
	}

	wlr_log(L_ERROR, "Could not get icon from item");
	return;
}

/* Get an icon by its name */
static void reply_icon_name(DBusMessageIter *iter, void *_data, enum property_status status) {
	struct StatusNotifierItem *item = _data;

	if (status != PROP_EXISTS) {
		dbus_get_prop_async(item->name, item->object_path,
			(item->kde_special_snowflake ? KDE_IFACE : FD_IFACE),
			"IconPixmap", "a(iiay)", reply_icon, item);
		return;
	}

	char *icon_name;
	dbus_message_iter_get_basic(iter, &icon_name);

	cairo_surface_t *image = find_icon(icon_name, 256);

	if (image) {
		wlr_log(L_DEBUG, "Icon for %s found with size %d", icon_name,
				cairo_image_surface_get_width(image));
		if (item->image) {
			cairo_surface_destroy(item->image);
		}
		item->image = image;
		item->dirty = true;
		// TODO TRAY rerender
		//dirty = true;

		return;
	}

	// Now try the pixmap
	dbus_get_prop_async(item->name, item->object_path,
		(item->kde_special_snowflake ? KDE_IFACE : FD_IFACE),
		"IconPixmap", "a(iiay)", reply_icon, item);
}

void get_icon(struct StatusNotifierItem *item) {
	dbus_get_prop_async(item->name, item->object_path,
		(item->kde_special_snowflake ? KDE_IFACE : FD_IFACE),
		"IconName", "s", reply_icon_name, item);
}

void sni_activate(struct StatusNotifierItem *item, uint32_t x, uint32_t y) {
	const char *iface =
		(item->kde_special_snowflake ? "org.kde.StatusNotifierItem"
		 : "org.freedesktop.StatusNotifierItem");
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			item->object_path,
			iface,
			"Activate");

	dbus_message_append_args(message,
			DBUS_TYPE_INT32, &x,
			DBUS_TYPE_INT32, &y,
			DBUS_TYPE_INVALID);

	dbus_connection_send(conn, message, NULL);

	dbus_message_unref(message);
}

void sni_context_menu(struct StatusNotifierItem *item, uint32_t x, uint32_t y) {
	const char *iface =
		(item->kde_special_snowflake ? "org.kde.StatusNotifierItem"
		 : "org.freedesktop.StatusNotifierItem");
	wlr_log(L_INFO, "Activating context menu for item: (%s,%s)", item->name, item->object_path);
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			item->object_path,
			iface,
			"ContextMenu");

	dbus_message_append_args(message,
			DBUS_TYPE_INT32, &x,
			DBUS_TYPE_INT32, &y,
			DBUS_TYPE_INVALID);

	dbus_connection_send(conn, message, NULL);

	dbus_message_unref(message);
}
void sni_secondary(struct StatusNotifierItem *item, uint32_t x, uint32_t y) {
	const char *iface =
		(item->kde_special_snowflake ? "org.kde.StatusNotifierItem"
		 : "org.freedesktop.StatusNotifierItem");
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			item->object_path,
			iface,
			"SecondaryActivate");

	dbus_message_append_args(message,
			DBUS_TYPE_INT32, &x,
			DBUS_TYPE_INT32, &y,
			DBUS_TYPE_INVALID);

	dbus_connection_send(conn, message, NULL);

	dbus_message_unref(message);
}

static void get_unique_name(struct StatusNotifierItem *item) {
	// I think that we're fine being sync here becaues the message is
	// directly to the message bus. Could be async though.
	DBusMessage *message = dbus_message_new_method_call(
			"org.freedesktop.DBus",
			"/org/freedesktop/DBus",
			"org.freedesktop.DBus",
			"GetNameOwner");

	dbus_message_append_args(message,
			DBUS_TYPE_STRING, &item->name,
			DBUS_TYPE_INVALID);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(
			conn, message, -1, NULL);

	dbus_message_unref(message);

	if (!reply) {
		wlr_log(L_ERROR, "Could not get unique name for item: %s",
				item->name);
		return;
	}

	char *unique_name;
	if (!dbus_message_get_args(reply, NULL,
				DBUS_TYPE_STRING, &unique_name,
				DBUS_TYPE_INVALID)) {
		wlr_log(L_ERROR, "Error parsing method args");
	} else {
		if (item->unique_name) {
			free(item->unique_name);
		}
		item->unique_name = strdup(unique_name);
	}

	dbus_message_unref(reply);
}

struct StatusNotifierItem *sni_create(const char *name) {
	// Make sure `name` is well formed
	if (!dbus_validate_bus_name(name, NULL)) {
		wlr_log(L_INFO, "Name (%s) is not a bus name. We cannot create an item.", name);
		return NULL;
	}

	struct StatusNotifierItem *item = malloc(sizeof(struct StatusNotifierItem));
	item->name = strdup(name);
	item->unique_name = NULL;
	// TODO use static str if the default path instead of all these god-damn strdups
	item->object_path = strdup("/StatusNotifierItem");
	item->image = NULL;
	item->dirty = false;

	// If it doesn't use this name then assume that it uses the KDE spec
	// This is because xembed-sni-proxy uses neither "org.freedesktop" nor
	// "org.kde" and just gives us the items "unique name"
	//
	// We could use this to our advantage and fill out the "unique name"
	// field with the given name if it is neither freedesktop or kde, but
	// that's makes us rely on KDE hackyness which is bad practice
	const char freedesktop_name[] = "org.freedesktop";
	if (strncmp(name, freedesktop_name, sizeof(freedesktop_name) - 1) != 0) {
		item->kde_special_snowflake = true;
	} else {
		item->kde_special_snowflake = false;
	}

	get_icon(item);

	get_unique_name(item);

	return item;
}
struct StatusNotifierItem *sni_create_from_obj_path(const char *unique_name,
		const char *object_path) {
	struct StatusNotifierItem *item = malloc(sizeof(struct StatusNotifierItem));
	// XXX strdup-ing twice to avoid a double-free; see above todo
	item->name = strdup(unique_name);
	item->unique_name = strdup(unique_name);
	item->object_path = strdup(object_path);
	item->image = NULL;
	item->dirty = false;
	// If they're registering by obj-path they're a special snowflake
	item->kde_special_snowflake = true;

	get_icon(item);
	return item;
}
void sni_free(struct StatusNotifierItem *item) {
	if (!item) {
		return;
	}
	free(item->name);
	free(item->unique_name);
	free(item->object_path);
	if (item->image) {
		cairo_surface_destroy(item->image);
	}
	free(item);
}
