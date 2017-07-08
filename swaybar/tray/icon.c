#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include "swaybar/tray/icon.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "stringop.h"
#include "log.h"

/**
 * REVIEW:
 * This file repeats lots of "costly" operations that are the same for every
 * icon. It's possible to create a dictionary or some other structure to cache
 * these, though it may complicate things somewhat.
 *
 * Also parsing (index.theme) is currently pretty messy, so that could be made
 * much better as well. Over all, things work, but are not optimal.
 */

/* Finds all themes that the given theme inherits */
static list_t *find_inherits(const char *theme_dir) {
	const char inherits[] = "Inherits";
	const char index_name[] = "index.theme";
	list_t *themes = create_list();
	FILE *index = NULL;
	char *path = malloc(strlen(theme_dir) + sizeof(index_name));
	if (!path) {
		goto fail;
	}
	if (!themes) {
		goto fail;
	}

	strcpy(path, theme_dir);
	strcat(path, index_name);

	index = fopen(path, "r");
	if (!index) {
		goto fail;
	}

	char *buf = NULL;
	size_t n = 0;
	while (!feof(index) && getline(&buf, &n, index) != -1) {
		if (n <= sizeof(inherits) + 1) {
			continue;
		}
		if (strncmp(inherits, buf, sizeof(inherits) - 1) == 0) {
			char *themestr = buf + sizeof(inherits);
			themes = split_string(themestr, ",");
			break;
		}
	}
	free(buf);

fail:
	free(path);
	if (index) {
		fclose(index);
	}
	return themes;
}

static bool isdir(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != -1) {
		if (S_ISDIR(statbuf.st_mode)) {
			return true;
		}
	}
	return false;

}

/**
 * Returns the directory of a given theme if it exists.
 * The returned pointer must be freed.
 */
static char *find_theme_dir(const char *theme) {
	char *basedir;
	char *icon_dir;

	if (!theme) {
		return NULL;
	}

	if (!(icon_dir = malloc(1024))) {
		sway_log(L_ERROR, "Out of memory!");
		goto fail;
	}

	if ((basedir = getenv("HOME"))) {
		if (snprintf(icon_dir, 1024, "%s/.icons/%s", basedir, theme) >= 1024) {
			sway_log(L_ERROR, "Path too long to render");
			// XXX perhaps just goto trying in /usr/share? This
			// shouldn't happen anyway, but might with a long global
			goto fail;
		}

		if (isdir(icon_dir)) {
			return icon_dir;
		}
	}

	if ((basedir = getenv("XDG_DATA_DIRS"))) {
		if (snprintf(icon_dir, 1024, "%s/icons/%s", basedir, theme) >= 1024) {
			sway_log(L_ERROR, "Path too long to render");
			// ditto
			goto fail;
		}

		if (isdir(icon_dir)) {
			return icon_dir;
		}
	}

	// Spec says use "/usr/share/pixmaps/", but I see everything in
	// "/usr/share/icons/" look it both, I suppose.
	if (snprintf(icon_dir, 1024, "/usr/share/pixmaps/%s", theme) >= 1024) {
		sway_log(L_ERROR, "Path too long to render");
		goto fail;
	}
	if (isdir(icon_dir)) {
		return icon_dir;
	}

	if (snprintf(icon_dir, 1024, "/usr/share/icons/%s", theme) >= 1024) {
		sway_log(L_ERROR, "Path too long to render");
		goto fail;
	}
	if (isdir(icon_dir)) {
		return icon_dir;
	}

fail:
	free(icon_dir);
	sway_log(L_ERROR, "Could not find dir for theme: %s", theme);
	return NULL;
}

/**
 * Returns all theme dirs needed to be looked in for an icon.
 * Does not check for duplicates
 */
static list_t *find_all_theme_dirs(const char *theme) {
	list_t *dirs = create_list();
	if (!dirs) {
		return NULL;
	}
	char *dir = find_theme_dir(theme);
	if (dir) {
		list_add(dirs, dir);
		list_t *inherits = find_inherits(dir);
		list_cat(dirs, inherits);
		list_free(inherits);
	}
	dir = find_theme_dir("hicolor");
	if (dir) {
		list_add(dirs, dir);
	}

	return dirs;
}

struct subdir {
	int size;
	char name[];
};

static int subdir_str_cmp(const void *_subdir, const void *_str) {
	const struct subdir *subdir = _subdir;
	const char *str = _str;
	return strcmp(subdir->name, str);
}
/**
 * Helper to find_subdirs. Acts similar to `split_string(subdirs, ",")` but
 * generates a list of struct subdirs
 */
static list_t *split_subdirs(char *subdir_str) {
	list_t *subdir_list = create_list();
	char *copy = strdup(subdir_str);
	if (!subdir_list || !copy) {
		list_free(subdir_list);
		free(copy);
		return NULL;
	}

	char *token;
	token = strtok(copy, ",");
	while(token) {
		int len = strlen(token) + 1;
		struct subdir *subdir =
			malloc(sizeof(struct subdir) + sizeof(char [len]));
		if (!subdir) {
			// Return what we have
			return subdir_list;
		}
		subdir->size = 0;
		strcpy(subdir->name, token);

		list_add(subdir_list, subdir);

		token = strtok(NULL, ",");
	}
	free(copy);

	return subdir_list;
}
/**
 * Returns a list of all subdirectories of a theme.
 * Take note: the subdir names are all relative to `theme_dir` and must be
 * combined with it to form a valid directory.
 *
 * Each member of the list is of type (struct subdir *) this struct contains
 * the name of the subdir, along with size information. These must be freed
 * bye the caller.
 *
 * This currently ignores min and max sizes of icons.
 */
static list_t* find_theme_subdirs(const char *theme_dir) {
	const char index_name[] = "/index.theme";
	list_t *dirs = NULL;
	char *path = malloc(strlen(theme_dir) + sizeof(index_name));
	FILE *index = NULL;
	if (!path) {
		sway_log(L_ERROR, "Failed to allocate memory");
		goto fail;
	}

	strcpy(path, theme_dir);
	strcat(path, index_name);

	index = fopen(path, "r");
	if (!index) {
		sway_log(L_ERROR, "Could not open file: %s", path);
		goto fail;
	}

	char *buf = NULL;
	size_t n = 0;
	const char directories[] = "Directories";
	while (!feof(index) && getline(&buf, &n, index) != -1) {
		if (n <= sizeof(directories) + 1) {
			continue;
		}
		if (strncmp(directories, buf, sizeof(directories) - 1) == 0) {
			char *dirstr = buf + sizeof(directories);
			dirs = split_subdirs(dirstr);
			break;
		}
	}
	// Now, find the size of each dir
	struct subdir *current_subdir = NULL;
	const char size[] = "Size";
	while (!feof(index) && getline(&buf, &n, index) != -1) {
		if (buf[0] == '[') {
			int len = strlen(buf);
			if (buf[len-1] == '\n') {
				len--;
			}
			// replace ']'
			buf[len-1] = '\0';

			int index;
			if ((index = list_seq_find(dirs, subdir_str_cmp, buf+1)) != -1) {
				current_subdir = (dirs->items[index]);
			}
		}

		if (strncmp(size, buf, sizeof(size) - 1) == 0) {
			if (current_subdir) {
				current_subdir->size = atoi(buf + sizeof(size));
			}
		}
	}
	free(buf);
fail:
	free(path);
	if (index) {
		fclose(index);
	}
	return dirs;
}

/* Returns the file of an icon given its name and size */
static char *find_icon_file(const char *name, int size) {
	int namelen = strlen(name);
	list_t *dirs = find_all_theme_dirs(swaybar.config->icon_theme);
	if (!dirs) {
		return NULL;
	}
	int min_size_diff = INT_MAX;
	char *current_file = NULL;

	for (int i = 0; i < dirs->length; ++i) {
		char *dir = dirs->items[i];
		list_t *subdirs = find_theme_subdirs(dir);

		if (!subdirs) {
			continue;
		}

		for (int i = 0; i < subdirs->length; ++i) {
			struct subdir *subdir = subdirs->items[i];

			// Only use an unsized if we don't already have a
			// canidate this should probably change to allow svgs
			if (!subdir->size && current_file) {
				continue;
			}

			int size_diff = abs(size - subdir->size);

			if (size_diff >= min_size_diff) {
				continue;
			}

			char *path = malloc(strlen(subdir->name) + strlen(dir) + 2);

			strcpy(path, dir);
			path[strlen(dir)] = '/';
			strcpy(path + strlen(dir) + 1, subdir->name);

			DIR *icons = opendir(path);
			if (!icons) {
				free(path);
				continue;
			}

			struct dirent *direntry;
			while ((direntry = readdir(icons)) != NULL) {
				int len = strlen(direntry->d_name);
				if (len <= namelen + 2) { //must have some ext
					continue;
				}
				if (strncmp(direntry->d_name, name, namelen) == 0) {
					char *ext = direntry->d_name + namelen + 1;
#ifdef WITH_GDK_PIXBUF
					if (strcmp(ext, "png") == 0 ||
							strcmp(ext, "xpm") == 0 ||
							strcmp(ext, "svg") == 0) {
#else
					if (strcmp(ext, "png") == 0) {
#endif
						free(current_file);
						char *icon_path = malloc(strlen(path) + len + 2);

						strcpy(icon_path, path);
						icon_path[strlen(path)] = '/';
						strcpy(icon_path + strlen(path) + 1, direntry->d_name);
						current_file = icon_path;
						min_size_diff = size_diff;
					}
				}
			}
			free(path);
			closedir(icons);
		}
		free_flat_list(subdirs);
	}
	free_flat_list(dirs);

	return current_file;
}

cairo_surface_t *find_icon(const char *name, int size) {
	char *image_path = find_icon_file(name, size);
	if (image_path == NULL) {
		return NULL;
	}

	cairo_surface_t *image = NULL;
#ifdef WITH_GDK_PIXBUF
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(image_path, &err);
	if (!pixbuf) {
		sway_log(L_ERROR, "Failed to load icon image: %s", err->message);
	}
	image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
#else
	// TODO make svg work? cairo supports it. maybe remove gdk alltogether
	image = cairo_image_surface_create_from_png(image_path);
#endif //WITH_GDK_PIXBUF
	if (!image) {
		sway_log(L_ERROR, "Could not read icon image");
		return NULL;
	}

	free(image_path);
	return image;
}
