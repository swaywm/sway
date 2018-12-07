#ifndef _SWAYBAR_TRAY_ITEM_H
#define _SWAYBAR_TRAY_ITEM_H

#include <stdbool.h>
#include "swaybar/tray/tray.h"
#include "list.h"

struct swaybar_pixmap {
	int size;
	unsigned char pixels[];
};

struct swaybar_sni {
	// icon properties
	struct swaybar_tray *tray;
	cairo_surface_t *icon;
	int min_size;
	int max_size;

	// dbus properties
	char *watcher_id;
	char *service;
	char *path;
	char *interface;

	char *status;
	char *icon_name;
	list_t *icon_pixmap; // struct swaybar_pixmap *
	char *attention_icon_name;
	list_t *attention_icon_pixmap; // struct swaybar_pixmap *
	bool item_is_menu;
	char *menu;
};

struct swaybar_sni *create_sni(char *id, struct swaybar_tray *tray);
void destroy_sni(struct swaybar_sni *sni);

#endif
