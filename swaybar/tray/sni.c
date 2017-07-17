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
#include "client/cairo.h"
#include "log.h"

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
static void reply_icon(DBusPendingCall *pending, void *_data) {
	struct StatusNotifierItem *item = _data;

	DBusMessage *reply = dbus_pending_call_steal_reply(pending);

	if (!reply) {
		sway_log(L_ERROR, "Did not get reply");
		goto bail;
	}

	int message_type = dbus_message_get_type(reply);

	if (message_type == DBUS_MESSAGE_TYPE_ERROR) {
		char *msg;

		dbus_message_get_args(reply, NULL,
				DBUS_TYPE_STRING, &msg,
				DBUS_TYPE_INVALID);

		sway_log(L_ERROR, "Message is error: %s", msg);
		goto bail;
	}

	DBusMessageIter iter;
	DBusMessageIter variant; /* v[a(iiay)] */
	DBusMessageIter array; /* a(iiay) */
	DBusMessageIter d_struct; /* (iiay) */
	DBusMessageIter icon; /* ay */

	dbus_message_iter_init(reply, &iter);

	// Each if here checks the types above before recursing
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
		sway_log(L_ERROR, "Relpy type incorrect");
		sway_log(L_ERROR, "Should be \"v\", is \"%s\"",
				dbus_message_iter_get_signature(&iter));
		goto bail;
	}
	dbus_message_iter_recurse(&iter, &variant);

	if (strcmp("a(iiay)", dbus_message_iter_get_signature(&variant)) != 0) {
		sway_log(L_ERROR, "Relpy type incorrect");
		sway_log(L_ERROR, "Should be \"a(iiay)\", is \"%s\"",
				dbus_message_iter_get_signature(&variant));
		goto bail;
	}

	if (dbus_message_iter_get_element_count(&variant) == 0) {
		// Can't recurse if there are no items
		sway_log(L_INFO, "Item has no icon");
		goto bail;
	}
	dbus_message_iter_recurse(&variant, &array);

	dbus_message_iter_recurse(&array, &d_struct);

	int width;
	dbus_message_iter_get_basic(&d_struct, &width);
	dbus_message_iter_next(&d_struct);

	int height;
	dbus_message_iter_get_basic(&d_struct, &height);
	dbus_message_iter_next(&d_struct);

	int len = dbus_message_iter_get_element_count(&d_struct);

	if (!len) {
		sway_log(L_ERROR, "No icon data");
		goto bail;
	}

	// Also implies len % 4 == 0, useful below
	if (len != width * height * 4) {
		sway_log(L_ERROR, "Incorrect array size passed");
		goto bail;
	}

	dbus_message_iter_recurse(&d_struct, &icon);

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	// FIXME support a variable stride
	// (works on my machine though for all tested widths)
	if (!sway_assert(stride == width * 4, "Stride must be equal to byte length")) {
		goto bail;
	}

	// Data is by reference, no need to free
	uint8_t *message_data;
	dbus_message_iter_get_fixed_array(&icon, &message_data, &len);

	uint8_t *image_data = malloc(stride * height);
	if (!image_data) {
		sway_log(L_ERROR, "Could not allocate memory for icon");
		goto bail;
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
		dirty = true;

		dbus_message_unref(reply);
		dbus_pending_call_unref(pending);
		return;
	} else {
		sway_log(L_ERROR, "Could not create image surface");
		free(image_data);
	}

bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	dbus_pending_call_unref(pending);
	sway_log(L_ERROR, "Could not get icon from item");
	return;
}
static void send_icon_msg(struct StatusNotifierItem *item) {
	DBusPendingCall *pending;
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			"/StatusNotifierItem",
			"org.freedesktop.DBus.Properties",
			"Get");
	const char *iface;
	if (item->kde_special_snowflake) {
		iface = "org.kde.StatusNotifierItem";
	} else {
		iface = "org.freedesktop.StatusNotifierItem";
	}
	const char *prop = "IconPixmap";

	dbus_message_append_args(message,
			DBUS_TYPE_STRING, &iface,
			DBUS_TYPE_STRING, &prop,
			DBUS_TYPE_INVALID);

	bool status =
		dbus_connection_send_with_reply(conn, message, &pending, -1);

	dbus_message_unref(message);

	if (!(pending || status)) {
		sway_log(L_ERROR, "Could not get item icon");
		return;
	}

	dbus_pending_call_set_notify(pending, reply_icon, item, NULL);
}

/* Get an icon by its name */
static void reply_icon_name(DBusPendingCall *pending, void *_data) {
	struct StatusNotifierItem *item = _data;

	DBusMessage *reply = dbus_pending_call_steal_reply(pending);

	if (!reply) {
		sway_log(L_INFO, "Got no icon name reply from item");
		goto bail;
	}

	int message_type = dbus_message_get_type(reply);

	if (message_type == DBUS_MESSAGE_TYPE_ERROR) {
		char *msg;

		dbus_message_get_args(reply, NULL,
				DBUS_TYPE_STRING, &msg,
				DBUS_TYPE_INVALID);

		sway_log(L_INFO, "Could not get icon name: %s", msg);
		goto bail;
	}

	DBusMessageIter iter; /* v[s] */
	DBusMessageIter variant; /* s */

	dbus_message_iter_init(reply, &iter);
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
		sway_log(L_ERROR, "Relpy type incorrect");
		sway_log(L_ERROR, "Should be \"v\", is \"%s\"",
				dbus_message_iter_get_signature(&iter));
		goto bail;
	}
	dbus_message_iter_recurse(&iter, &variant);


	if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_STRING) {
		sway_log(L_ERROR, "Relpy type incorrect");
		sway_log(L_ERROR, "Should be \"s\", is \"%s\"",
				dbus_message_iter_get_signature(&iter));
		goto bail;
	}

	char *icon_name;
	dbus_message_iter_get_basic(&variant, &icon_name);

	cairo_surface_t *image = find_icon(icon_name, 256);

	if (image) {
		sway_log(L_DEBUG, "Icon for %s found with size %d", icon_name,
				cairo_image_surface_get_width(image));
		if (item->image) {
			cairo_surface_destroy(item->image);
		}
		item->image = image;
		item->dirty = true;
		dirty = true;

		dbus_message_unref(reply);
		dbus_pending_call_unref(pending);
		return;
	}

bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	dbus_pending_call_unref(pending);
	// Now try the pixmap
	send_icon_msg(item);
	return;
}
static void send_icon_name_msg(struct StatusNotifierItem *item) {
	DBusPendingCall *pending;
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			"/StatusNotifierItem",
			"org.freedesktop.DBus.Properties",
			"Get");
	const char *iface;
	if (item->kde_special_snowflake) {
		iface = "org.kde.StatusNotifierItem";
	} else {
		iface = "org.freedesktop.StatusNotifierItem";
	}
	const char *prop = "IconName";

	dbus_message_append_args(message,
			DBUS_TYPE_STRING, &iface,
			DBUS_TYPE_STRING, &prop,
			DBUS_TYPE_INVALID);

	bool status =
		dbus_connection_send_with_reply(conn, message, &pending, -1);

	dbus_message_unref(message);

	if (!(pending || status)) {
		sway_log(L_ERROR, "Could not get item icon name");
		return;
	}

	dbus_pending_call_set_notify(pending, reply_icon_name, item, NULL);
}

void get_icon(struct StatusNotifierItem *item) {
	send_icon_name_msg(item);
}

void sni_activate(struct StatusNotifierItem *item, uint32_t x, uint32_t y) {
	const char *iface =
		(item->kde_special_snowflake ? "org.kde.StatusNotifierItem"
		 : "org.freedesktop.StatusNotifierItem");
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			"/StatusNotifierItem",
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
	DBusMessage *message = dbus_message_new_method_call(
			item->name,
			"/StatusNotifierItem",
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
			"/StatusNotifierItem",
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
		sway_log(L_ERROR, "Could not get unique name for item: %s",
				item->name);
		return;
	}

	char *unique_name;
	if (!dbus_message_get_args(reply, NULL,
				DBUS_TYPE_STRING, &unique_name,
				DBUS_TYPE_INVALID)) {
		sway_log(L_ERROR, "Error parsing method args");
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
		sway_log(L_INFO, "Name (%s) is not a bus name. We cannot create an item.", name);
		return NULL;
	}

	struct StatusNotifierItem *item = malloc(sizeof(struct StatusNotifierItem));
	item->name = strdup(name);
	item->unique_name = NULL;
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
/* Return 0 if `item` has a name of `str` */
int sni_str_cmp(const void *_item, const void *_str) {
	const struct StatusNotifierItem *item = _item;
	const char *str = _str;

	return strcmp(item->name, str);
}
/* Returns 0 if `item` has a unique name of `str` */
int sni_uniq_cmp(const void *_item, const void *_str) {
	const struct StatusNotifierItem *item = _item;
	const char *str = _str;

	if (!item->unique_name) {
		return false;
	}
	return strcmp(item->unique_name, str);
}
void sni_free(struct StatusNotifierItem *item) {
	if (!item) {
		return;
	}
	free(item->name);
	if (item->unique_name) {
		free(item->unique_name);
	}
	if (item->image) {
		cairo_surface_destroy(item->image);
	}
	free(item);
}
