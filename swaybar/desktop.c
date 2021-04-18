#define _POSIX_C_SOURCE 200809
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "swaybar/tray/icon.h"
#include "desktop.h"
#include "log.h"

static list_t *get_desktop_files_basedirs() {
	list_t *basedirs = create_list();
	// TODO: Get correct list of directories
	list_add(basedirs, "/usr/share/applications");
	return basedirs;
}

static char *load_desktop_entry(const char *app_name, list_t *basedirs) {
	assert(app_name);
	assert(basedirs);

	size_t desktop_filename_len = snprintf(NULL, 0, "%s.desktop", app_name) + 1;
	char *desktop_filename = malloc(desktop_filename_len);
	snprintf(desktop_filename, desktop_filename_len, "%s.desktop", app_name);

	for (int i = 0; i < basedirs->length; ++i) {
		const char *basedir = basedirs->items[i];

		DIR *d;
		struct dirent *dir;
		d = opendir(basedir);
		if (d) {
			while ((dir = readdir(d)) != NULL) {
				if (strcmp(desktop_filename, dir->d_name) == 0) {
					char *buf = append_path_safe(basedir, desktop_filename);

					FILE *f = fopen(buf, "rb");
					assert(f);
					fseek(f, 0, SEEK_END);
					long fsize = ftell(f);
					fseek(f, 0, SEEK_SET); /* same as rewind(f); */

					char *string = malloc(fsize + 1);
					fread(string, 1, fsize, f);
					fclose(f);

					string[fsize] = 0;
					closedir(d);

					return string;
				}
			}
			closedir(d);
		}
	}

	return NULL;
}

char *load_desktop_entry_from_xdgdirs(const char *app_name) {
	list_t *basedirs = get_desktop_files_basedirs();
	return load_desktop_entry(app_name, basedirs);
}

char *get_icon_name_from_desktop_entry(const char *desktop_entry) {
	if (!desktop_entry) {
		return NULL;
	}

	char *desktop_entry_start = strdup(desktop_entry);
	char *cur_line = desktop_entry_start;
	char *icon_name = NULL;
	while (cur_line) {
		char *next_line = strchr(cur_line, '\n');
		if (next_line) {
			*next_line = 0;
		}

		if (strncmp(cur_line, "Icon=", 5) == 0) {
			const char *icon_name_start = strchr(cur_line, '=') + 1;
			icon_name = strdup(icon_name_start);
			break;
		}

		if (next_line) {
			*next_line = '\n';
		}
		cur_line = next_line ? (next_line + 1) : NULL;
	}
	free(desktop_entry_start);

	return icon_name;
}
