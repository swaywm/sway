#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <dbus/dbus.h>
#include "swaybar/bar.h"
#include "swaybar/tray/tray.h"
#include "swaybar/tray/dbus.h"
#include "swaybar/tray/sni.h"
#include "swaybar/tray/sni_watcher.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "list.h"
#include "log.h"

struct tray *tray;

static void register_host(char *name) {
	DBusMessage *message;

	message = dbus_message_new_method_call(
			"org.freedesktop.StatusNotifierWatcher",
			"/StatusNotifierWatcher",
			"org.freedesktop.StatusNotifierWatcher",
			"RegisterStatusNotifierHost");
	if (!message) {
		sway_log(L_ERROR, "Cannot allocate dbus method call");
		return;
	}

	dbus_message_append_args(message,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INVALID);

	dbus_connection_send(conn, message, NULL);

	dbus_message_unref(message);
}

static void get_items_reply(DBusMessageIter *iter, void *_data, enum property_status status) {
	if (status != PROP_EXISTS) {
		return;
	}
	DBusMessageIter array;

	// O(n) function, could be faster dynamically reading values
	int len = dbus_message_iter_get_element_count(iter);

	dbus_message_iter_recurse(iter, &array);
	for (int i = 0; i < len; i++) {
		const char *name;
		dbus_message_iter_get_basic(&array, &name);

		if (list_seq_find(tray->items, sni_str_cmp, name) == -1) {
			struct StatusNotifierItem *item = sni_create(name);

			if (item) {
				sway_log(L_DEBUG, "Item registered with host: %s", name);
				list_add(tray->items, item);
				dirty = true;
			}
		}
	}
}
static void get_obj_items_reply(DBusMessageIter *iter, void *_data, enum property_status status) {
	if (status != PROP_EXISTS) {
		return;
	}
	DBusMessageIter array;
	DBusMessageIter dstruct;

	int len = dbus_message_iter_get_element_count(iter);

	dbus_message_iter_recurse(iter, &array);
	for (int i = 0; i < len; i++) {
		const char *object_path;
		const char *unique_name;

		dbus_message_iter_recurse(&array, &dstruct);

		dbus_message_iter_get_basic(&dstruct, &object_path);
		dbus_message_iter_next(&dstruct);
		dbus_message_iter_get_basic(&dstruct, &unique_name);

		struct ObjName obj_name = {
			object_path,
			unique_name,
		};
		if (list_seq_find(tray->items, sni_obj_name_cmp, &obj_name) == -1) {
			struct StatusNotifierItem *item =
				sni_create_from_obj_path(unique_name, object_path);

			if (item) {
				sway_log(L_DEBUG, "Item registered with host: %s", unique_name);
				list_add(tray->items, item);
				dirty = true;
			}
		}
	}
}

static void get_items() {
	// Clear list
	list_foreach(tray->items, (void (*)(void *))sni_free);
	list_free(tray->items);
	tray->items = create_list();

	dbus_get_prop_async("org.freedesktop.StatusNotifierWatcher",
		"/StatusNotifierWatcher","org.freedesktop.StatusNotifierWatcher",
		"RegisteredStatusNotifierItems", "as", get_items_reply, NULL);

	dbus_get_prop_async("org.freedesktop.StatusNotifierWatcher",
		"/StatusNotifierWatcher","org.swaywm.LessSuckyStatusNotifierWatcher",
		"RegisteredObjectPathItems", "a(os)", get_obj_items_reply, NULL);
}

static DBusHandlerResult signal_handler(DBusConnection *connection,
		DBusMessage *message, void *_data) {
	if (dbus_message_is_signal(message, "org.freedesktop.StatusNotifierWatcher",
				"StatusNotifierItemRegistered")) {
		const char *name;
		if (!dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_INVALID)) {
			sway_log(L_ERROR, "Error getting StatusNotifierItemRegistered args");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (list_seq_find(tray->items, sni_str_cmp, name) == -1) {
			struct StatusNotifierItem *item = sni_create(name);

			if (item) {
				list_add(tray->items, item);
				dirty = true;
			}
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, "org.freedesktop.StatusNotifierWatcher",
				"StatusNotifierItemUnregistered")) {
		const char *name;
		if (!dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_INVALID)) {
			sway_log(L_ERROR, "Error getting StatusNotifierItemUnregistered args");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		int index;
		bool found_item = false;
		while ((index = list_seq_find(tray->items, sni_str_cmp, name)) != -1) {
			found_item = true;
			sni_free(tray->items->items[index]);
			list_del(tray->items, index);
			dirty = true;
		}
		if (found_item == false) {
			// If it's not in our list, then our list is incorrect.
			// Fetch all items again
			sway_log(L_INFO, "Host item list incorrect, refreshing");
			get_items();
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, "org.freedesktop.StatusNotifierItem",
				"NewIcon") || dbus_message_is_signal(message,
				"org.kde.StatusNotifierItem", "NewIcon")) {
		const char *name;
		const char *obj_path;
		int index;
		struct StatusNotifierItem *item;

		name = dbus_message_get_sender(message);
		obj_path = dbus_message_get_path(message);
		struct ObjName obj_name = {
			obj_path,
			name,
		};
		if ((index = list_seq_find(tray->items, sni_obj_name_cmp, &obj_name)) != -1) {
			item = tray->items->items[index];
			sway_log(L_INFO, "NewIcon signal from item %s", item->name);
			get_icon(item);
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message,
				"org.swaywm.LessSuckyStatusNotifierWatcher",
				"ObjPathItemRegistered")) {
		const char *object_path;
		const char *unique_name;
		if (!dbus_message_get_args(message, NULL,
				DBUS_TYPE_OBJECT_PATH, &object_path,
				DBUS_TYPE_STRING, &unique_name,
				DBUS_TYPE_INVALID)) {
			sway_log(L_ERROR, "Error getting ObjPathItemRegistered args");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		struct ObjName obj_name = {
			object_path,
			unique_name,
		};
		if (list_seq_find(tray->items, sni_obj_name_cmp, &obj_name) == -1) {
			struct StatusNotifierItem *item =
				sni_create_from_obj_path(unique_name,
						object_path);

			if (item) {
				list_add(tray->items, item);
				dirty = true;
			}
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int init_host() {
	tray = (struct tray *)malloc(sizeof(tray));

	tray->items = create_list();

	DBusError error;
	dbus_error_init(&error);
	char *name = NULL;
	if (!conn) {
		sway_log(L_ERROR, "Connection is null, cannot init SNI host");
		goto err;
	}
	name = calloc(sizeof(char), 256);

	if (!name) {
		sway_log(L_ERROR, "Cannot allocate name");
		goto err;
	}

	pid_t pid = getpid();
	if (snprintf(name, 256, "org.freedesktop.StatusNotifierHost-%d", pid)
			>= 256) {
		sway_log(L_ERROR, "Cannot get host name because string is too short."
				"This should not happen");
		goto err;
	}

	// We want to be the sole owner of this name
	if (dbus_bus_request_name(conn, name, DBUS_NAME_FLAG_DO_NOT_QUEUE,
			&error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		sway_log(L_ERROR, "Cannot get host name and start the tray");
		goto err;
	}
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "Dbus err getting host name: %s\n", error.message);
		goto err;
	}
	sway_log(L_DEBUG, "Got host name");

	register_host(name);

	// Chances are if an item is already running, we'll get it two times.
	// Once from this and another time from queued signals. Still we want
	// to do this to be a complient sni host just in case.
	get_items();

	// Perhaps use addmatch helper functions like wlc does?
	dbus_bus_add_match(conn,
			"type='signal',\
			sender='org.freedesktop.StatusNotifierWatcher',\
			member='StatusNotifierItemRegistered'",
			&error);
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "dbus_err: %s", error.message);
		goto err;
	}
	dbus_bus_add_match(conn,
			"type='signal',\
			sender='org.freedesktop.StatusNotifierWatcher',\
			member='StatusNotifierItemUnregistered'",
			&error);
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "dbus_err: %s", error.message);
		return -1;
	}
	dbus_bus_add_match(conn,
			"type='signal',\
			sender='org.freedesktop.StatusNotifierWatcher',\
			member='ObjPathItemRegistered'",
			&error);
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "dbus_err: %s", error.message);
		return -1;
	}

	// SNI matches
	dbus_bus_add_match(conn,
			"type='signal',\
			interface='org.freedesktop.StatusNotifierItem',\
			member='NewIcon'",
			&error);
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "dbus_err %s", error.message);
		goto err;
	}
	dbus_bus_add_match(conn,
			"type='signal',\
			interface='org.kde.StatusNotifierItem',\
			member='NewIcon'",
			&error);
	if (dbus_error_is_set(&error)) {
		sway_log(L_ERROR, "dbus_err %s", error.message);
		goto err;
	}

	dbus_connection_add_filter(conn, signal_handler, NULL, NULL);

	free(name);
	return 0;

err:
	// TODO better handle errors
	free(name);
	return -1;
}

void tray_mouse_event(struct output *output, int rel_x, int rel_y,
		uint32_t button, uint32_t state) {

	int x = rel_x;
	int y = rel_y + (swaybar.config->position == DESKTOP_SHELL_PANEL_POSITION_TOP
			? 0 : (output->state->height - output->window->height));

	struct window *window = output->window;
	uint32_t tray_padding = swaybar.config->tray_padding;
	int tray_width = window->width * window->scale;

	for (int i = 0; i < output->items->length; ++i) {
		struct sni_icon_ref *item =
			 output->items->items[i];
		int icon_width = cairo_image_surface_get_width(item->icon);

		tray_width -= tray_padding;
		if (x <= tray_width && x >= tray_width - icon_width) {
			if (button == swaybar.config->activate_button) {
				sni_activate(item->ref, x, y);
			} else if (button == swaybar.config->context_button) {
				sni_context_menu(item->ref, x, y);
			} else if (button == swaybar.config->secondary_button) {
				sni_secondary(item->ref, x, y);
			}
			break;
		}
		tray_width -= icon_width;
	}
}

uint32_t tray_render(struct output *output, struct config *config) {
	struct window *window = output->window;
	cairo_t *cairo = window->cairo;

	// Tray icons
	uint32_t tray_padding = config->tray_padding;
	uint32_t tray_width = window->width * window->scale;
	const int item_size = (window->height * window->scale) - (2 * tray_padding);

	if (item_size < 0) {
		// Can't render items if the padding is too large
		return tray_width;
	}

	if (config->tray_output && strcmp(config->tray_output, output->name) != 0) {
		return tray_width;
	}

	bool clean_item = false;
	// Clean item if only one output has tray or this is the last output
	if (swaybar.outputs->length == 1 || config->tray_output || output->idx == swaybar.outputs->length-1) {
		clean_item = true;
	// More trickery is needed in case you plug off secondary outputs on live
	} else {
		int active_outputs = 0;
		for (int i = 0; i < swaybar.outputs->length; i++) {
			struct output *output = swaybar.outputs->items[i];
			if (output->active) {
				active_outputs++;
			}
		}
		if (active_outputs == 1) {
			clean_item = true;
		}
	}

	for (int i = 0; i < tray->items->length; ++i) {
		struct StatusNotifierItem *item =
			tray->items->items[i];
		if (!item->image) {
			continue;
		}

		struct sni_icon_ref *render_item = NULL;
		int j;
		for (j = i; j < output->items->length; ++j) {
			struct sni_icon_ref *ref =
				output->items->items[j];
			if (ref->ref == item) {
				render_item = ref;
				break;
			} else {
				sni_icon_ref_free(ref);
				list_del(output->items, j);
			}
		}

		if (!render_item) {
			render_item = sni_icon_ref_create(item, item_size);
			list_add(output->items, render_item);
		} else if (item->dirty) {
			// item needs re-render
			sway_log(L_DEBUG, "Redrawing item %d for output %d", i, output->idx);
			sni_icon_ref_free(render_item);
			output->items->items[j] = render_item =
				sni_icon_ref_create(item, item_size);
		}

		tray_width -= tray_padding;
		tray_width -= item_size;

		cairo_operator_t op = cairo_get_operator(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cairo, render_item->icon, tray_width, tray_padding);
		cairo_rectangle(cairo, tray_width, tray_padding, item_size, item_size);
		cairo_fill(cairo);
		cairo_set_operator(cairo, op);

		if (clean_item) {
			item->dirty = false;
		}
	}


	if (tray_width != window->width * window->scale) {
		tray_width -= tray_padding;
	}

	return tray_width;
}

void init_tray(struct bar *bar) {
	if (!bar->config->tray_output || strcmp(bar->config->tray_output, "none") != 0) {
		/* Connect to the D-Bus */
		dbus_init();

		/* Start the SNI watcher */
		init_sni_watcher();

		/* Start the SNI host */
		init_host();
	}
}
