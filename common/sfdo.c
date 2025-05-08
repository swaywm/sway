#include <sys/stat.h>
#include <stdlib.h>
#include <sfdo-basedir.h>
#include <sfdo-desktop.h>
#include <sfdo-icon.h>

#include "log.h"
#include "sfdo.h"

// this extends libsfdo's behavior to also handle icons specified as absolute paths
char *sfdo_icon_lookup_extended(struct sfdo *sfdo, char *icon_name, int target_size, int scale) {
	char *icon_path = NULL;
	if (icon_name[0] == '/') {
		struct stat sb;
		if (!stat(icon_name, &sb)) {
			icon_path = strdup(icon_name);
		}
	} else {
		int lookup_options = SFDO_ICON_THEME_LOOKUP_OPTIONS_DEFAULT;
		struct sfdo_icon_file *icon_file = \
			sfdo_icon_theme_lookup(sfdo->icon_theme, icon_name, SFDO_NT, \
				target_size, scale, lookup_options);
		if (icon_file && icon_file != SFDO_ICON_FILE_INVALID) {
			icon_path = strdup(sfdo_icon_file_get_path(icon_file, NULL));
		}
		sfdo_icon_file_destroy(icon_file);
	}
	return icon_path;
}

struct sfdo *sfdo_create(char *icon_theme) {
	if (!icon_theme) {
		goto error_null;
	}

	struct sfdo *sfdo = calloc(1, sizeof(struct sfdo));
	if (!sfdo) {
		goto error_calloc;
	}

	struct sfdo_basedir_ctx *basedir_ctx = sfdo_basedir_ctx_create();
	if (!basedir_ctx) {
		goto error_basedir_ctx;
	}

	sfdo->desktop_ctx = sfdo_desktop_ctx_create(basedir_ctx);
	if (!sfdo->desktop_ctx) {
		goto error_desktop_ctx;
	}

	sfdo->icon_ctx = sfdo_icon_ctx_create(basedir_ctx);
	if (!sfdo->icon_ctx) {
		goto error_icon_ctx;
	}

	sfdo->desktop_db = sfdo_desktop_db_load(sfdo->desktop_ctx, NULL);
	if (!sfdo->desktop_db) {
		goto error_desktop_db;
	}

	int load_options = SFDO_ICON_THEME_LOAD_OPTIONS_DEFAULT
		| SFDO_ICON_THEME_LOAD_OPTION_ALLOW_MISSING
		| SFDO_ICON_THEME_LOAD_OPTION_RELAXED;

	sfdo->icon_theme = sfdo_icon_theme_load(sfdo->icon_ctx, icon_theme, load_options);
	if (!sfdo->icon_theme) {
		goto error_icon_theme;
	}

	sfdo_basedir_ctx_destroy(basedir_ctx);

	sway_log(SWAY_INFO, "Successfully setup sfdo with icon theme %s", icon_theme);
	return sfdo;

error_icon_theme:
	sfdo_desktop_db_destroy(sfdo->desktop_db);
error_desktop_db:
	sfdo_icon_ctx_destroy(sfdo->icon_ctx);
error_icon_ctx:
	sfdo_desktop_ctx_destroy(sfdo->desktop_ctx);
error_desktop_ctx:
	sfdo_basedir_ctx_destroy(basedir_ctx);
error_basedir_ctx:
	free(sfdo);
error_calloc:
error_null:
	// it's safe to call with null
	sway_log(SWAY_ERROR, "Failed to setup sfdo with icon theme %s", icon_theme);
	return NULL;
}

void sfdo_destroy(struct sfdo *sfdo) {
	if (!sfdo) {
		sway_log(SWAY_DEBUG, "Null sfdo passed in");
		return;
	}

	sfdo_desktop_ctx_destroy(sfdo->desktop_ctx);
	sfdo_icon_ctx_destroy(sfdo->icon_ctx);
	sfdo_desktop_db_destroy(sfdo->desktop_db);
	sfdo_icon_theme_destroy(sfdo->icon_theme);
	free(sfdo);
	sway_log(SWAY_DEBUG, "Successfully destroyed sfdo");
}
