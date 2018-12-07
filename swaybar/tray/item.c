#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "swaybar/tray/host.h"
#include "swaybar/tray/item.h"
#include "swaybar/tray/tray.h"
#include "list.h"
#include "log.h"

// TODO menu

static int read_pixmap(sd_bus_message *msg, struct swaybar_sni *sni,
		const char *prop, list_t **dest) {
	int ret = sd_bus_message_enter_container(msg, 'a', "(iiay)");
	if (ret < 0) {
		wlr_log(WLR_DEBUG, "Failed to read property %s: %s", prop, strerror(-ret));
		return ret;
	}

	if (sd_bus_message_at_end(msg, 0)) {
		return ret;
	}

	list_t *pixmaps = create_list();
	if (!pixmaps) {
		return -12; // -ENOMEM
	}

	while (!sd_bus_message_at_end(msg, 0)) {
		ret = sd_bus_message_enter_container(msg, 'r', "iiay");
		if (ret < 0) {
			wlr_log(WLR_DEBUG, "Failed to read property %s: %s", prop, strerror(-ret));
			goto error;
		}

		int size;
		ret = sd_bus_message_read(msg, "ii", NULL, &size);
		if (ret < 0) {
			wlr_log(WLR_DEBUG, "Failed to read property %s: %s", prop, strerror(-ret));
			goto error;
		}

		const void *pixels;
		size_t npixels;
		ret = sd_bus_message_read_array(msg, 'y', &pixels, &npixels);
		if (ret < 0) {
			wlr_log(WLR_DEBUG, "Failed to read property %s: %s", prop, strerror(-ret));
			goto error;
		}

		struct swaybar_pixmap *pixmap =
			malloc(sizeof(struct swaybar_pixmap) + npixels);
		pixmap->size = size;
		memcpy(pixmap->pixels, pixels, npixels);
		list_add(pixmaps, pixmap);

		sd_bus_message_exit_container(msg);
	}
	*dest = pixmaps;

	return ret;
error:
	list_free_items_and_destroy(pixmaps);
	return ret;
}

struct get_property_data {
	struct swaybar_sni *sni;
	const char *prop;
	const char *type;
	void *dest;
};

static int get_property_callback(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct get_property_data *d = data;
	struct swaybar_sni *sni = d->sni;
	const char *prop = d->prop;
	const char *type = d->type;
	void *dest = d->dest;

	int ret;
	if (sd_bus_message_is_method_error(msg, NULL)) {
		sd_bus_error err = *sd_bus_message_get_error(msg);
		wlr_log(WLR_DEBUG, "Failed to get property %s: %s", prop, err.message);
		ret = -sd_bus_error_get_errno(&err);
		goto cleanup;
	}

	ret = sd_bus_message_enter_container(msg, 'v', type);
	if (ret < 0) {
		wlr_log(WLR_DEBUG, "Failed to read property %s: %s", prop, strerror(-ret));
		goto cleanup;
	}

	if (!type) {
		ret = read_pixmap(msg, sni, prop, dest);
		if (ret < 0) {
			goto cleanup;
		}
	} else {
		ret = sd_bus_message_read(msg, type, dest);
		if (ret < 0) {
			wlr_log(WLR_DEBUG, "Failed to read property %s: %s", prop,
					strerror(-ret));
			goto cleanup;
		} else if (*type == 's' || *type == 'o') {
			char **str = dest;
			*str = strdup(*str);
		}
	}
cleanup:
	free(data);
	return ret;
}

static void sni_get_property_async(struct swaybar_sni *sni, const char *prop,
		const char *type, void *dest) {
	struct get_property_data *data = malloc(sizeof(struct get_property_data));
	data->sni = sni;
	data->prop = prop;
	data->type = type;
	data->dest = dest;
	int ret = sd_bus_call_method_async(sni->tray->bus, NULL, sni->service,
			sni->path, "org.freedesktop.DBus.Properties", "Get",
			get_property_callback, data, "ss", sni->interface, prop);
	if (ret < 0) {
		wlr_log(WLR_DEBUG, "Failed to get property %s: %s", prop, strerror(-ret));
	}
}

static int handle_new_icon(sd_bus_message *msg, void *data, sd_bus_error *error) {
	struct swaybar_sni *sni = data;
	wlr_log(WLR_DEBUG, "%s has new IconName", sni->watcher_id);

	free(sni->icon_name);
	sni->icon_name = NULL;
	sni_get_property_async(sni, "IconName", "s", &sni->icon_name);

	list_free_items_and_destroy(sni->icon_pixmap);
	sni->icon_pixmap = NULL;
	sni_get_property_async(sni, "IconPixmap", NULL, &sni->icon_pixmap);

	return 0;
}

static int handle_new_attention_icon(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct swaybar_sni *sni = data;
	wlr_log(WLR_DEBUG, "%s has new AttentionIconName", sni->watcher_id);

	free(sni->attention_icon_name);
	sni->attention_icon_name = NULL;
	sni_get_property_async(sni, "AttentionIconName", "s", &sni->attention_icon_name);

	list_free_items_and_destroy(sni->attention_icon_pixmap);
	sni->attention_icon_pixmap = NULL;
	sni_get_property_async(sni, "AttentionIconPixmap", NULL, &sni->attention_icon_pixmap);

	return 0;
}

static int handle_new_status(sd_bus_message *msg, void *data, sd_bus_error *error) {
	char *status;
	int ret = sd_bus_message_read(msg, "s", &status);
	if (ret < 0) {
		wlr_log(WLR_DEBUG, "Failed to read new status message: %s", strerror(-ret));
	} else {
		struct swaybar_sni *sni = data;
		free(sni->status);
		sni->status = strdup(status);
		wlr_log(WLR_DEBUG, "%s has new Status '%s'", sni->watcher_id, status);
	}
	return ret;
}

static void sni_match_signal(struct swaybar_sni *sni, char *signal,
		sd_bus_message_handler_t callback) {
	int ret = sd_bus_match_signal(sni->tray->bus, NULL, sni->service, sni->path,
			sni->interface, signal, callback, sni);
	if (ret < 0) {
		wlr_log(WLR_DEBUG, "Failed to subscribe to signal %s: %s", signal,
				strerror(-ret));
	}
}

struct swaybar_sni *create_sni(char *id, struct swaybar_tray *tray) {
	struct swaybar_sni *sni = calloc(1, sizeof(struct swaybar_sni));
	if (!sni) {
		return NULL;
	}
	sni->tray = tray;
	sni->watcher_id = strdup(id);
	char *path_ptr = strchr(id, '/');
	if (!path_ptr) {
		sni->service = strdup(id);
		sni->path = strdup("/StatusNotifierItem");
		sni->interface = "org.freedesktop.StatusNotifierItem";
	} else {
		sni->service = strndup(id, path_ptr - id);
		sni->path = strdup(path_ptr);
		sni->interface = "org.kde.StatusNotifierItem";
	}

	// Ignored: Category, Id, Title, WindowId, OverlayIconName,
	//          OverlayIconPixmap, AttentionMovieName, ToolTip
	sni_get_property_async(sni, "Status", "s", &sni->status);
	sni_get_property_async(sni, "IconName", "s", &sni->icon_name);
	sni_get_property_async(sni, "IconPixmap", NULL, &sni->icon_pixmap);
	sni_get_property_async(sni, "AttentionIconName", "s", &sni->attention_icon_name);
	sni_get_property_async(sni, "AttentionIconPixmap", NULL, &sni->attention_icon_pixmap);
	sni_get_property_async(sni, "ItemIsMenu", "b", &sni->item_is_menu);
	sni_get_property_async(sni, "Menu", "o", &sni->menu);

	sni_match_signal(sni, "NewIcon", handle_new_icon);
	sni_match_signal(sni, "NewAttentionIcon", handle_new_attention_icon);
	sni_match_signal(sni, "NewStatus", handle_new_status);

	return sni;
}

void destroy_sni(struct swaybar_sni *sni) {
	if (!sni) {
		return;
	}

	free(sni->watcher_id);
	free(sni->service);
	free(sni->path);
	free(sni->status);
	free(sni->icon_name);
	free(sni->icon_pixmap);
	free(sni->attention_icon_name);
	free(sni->menu);
	free(sni);
}
