#ifndef _SWAY_SFDO_H
#define _SWAY_SFDO_H

#include <sfdo-desktop.h>
#include <sfdo-icon.h>
#include <sfdo-basedir.h>

struct sfdo {
	struct sfdo_desktop_ctx *desktop_ctx;
	struct sfdo_icon_ctx *icon_ctx;
	struct sfdo_desktop_db *desktop_db;
	struct sfdo_icon_theme *icon_theme;
};

char *sfdo_icon_lookup_extended(struct sfdo *sfdo, char *icon_name, int target_size, int scale);
struct sfdo *sfdo_create(char *icon_theme);
void sfdo_destroy(struct sfdo *sfdo);

#endif
