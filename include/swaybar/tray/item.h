#ifndef _SWAYBAR_TRAY_ITEM_H
#define _SWAYBAR_TRAY_ITEM_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>
#include "swaybar/tray/tray.h"
#include "list.h"

struct swaybar_output;

struct swaybar_pixmap {
	int size;
	unsigned char pixels[];
};

struct swaybar_sni_slot {
	struct wl_list link; // swaybar_sni::slots
	struct swaybar_sni *sni;
	const char *prop;
	const char *type;
	void *dest;
	sd_bus_slot *slot;
};

struct swaybar_sni {
	// icon properties
	struct swaybar_tray *tray;
	cairo_surface_t *icon;
	int min_size;
	int max_size;
	int target_size;

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
	char *icon_theme_path; // non-standard KDE property

	struct wl_list slots; // swaybar_sni_slot::link
};

struct swaybar_sni *create_sni(char *id, struct swaybar_tray *tray);
void destroy_sni(struct swaybar_sni *sni);
uint32_t render_sni(cairo_t *cairo, struct swaybar_output *output, double *x,
		struct swaybar_sni *sni);

#endif
