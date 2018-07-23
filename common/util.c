#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <xkbcommon/xkbcommon-names.h>
#include <wlr/types/wlr_keyboard.h>
#include "log.h"
#include "readline.h"
#include "util.h"

int wrap(int i, int max) {
	return ((i % max) + max) % max;
}

int numlen(int n) {
	if (n == 0) {
		return 1;
	}
	return log10(n) + 1;
}

static struct modifier_key {
	char *name;
	uint32_t mod;
} modifiers[] = {
	{ XKB_MOD_NAME_SHIFT, WLR_MODIFIER_SHIFT },
	{ XKB_MOD_NAME_CAPS, WLR_MODIFIER_CAPS },
	{ XKB_MOD_NAME_CTRL, WLR_MODIFIER_CTRL },
	{ "Ctrl", WLR_MODIFIER_CTRL },
	{ XKB_MOD_NAME_ALT, WLR_MODIFIER_ALT },
	{ "Alt", WLR_MODIFIER_ALT },
	{ XKB_MOD_NAME_NUM, WLR_MODIFIER_MOD2 },
	{ "Mod3", WLR_MODIFIER_MOD3 },
	{ XKB_MOD_NAME_LOGO, WLR_MODIFIER_LOGO },
	{ "Mod5", WLR_MODIFIER_MOD5 },
};

uint32_t get_modifier_mask_by_name(const char *name) {
	int i;
	for (i = 0; i < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++i) {
		if (strcasecmp(modifiers[i].name, name) == 0) {
			return modifiers[i].mod;
		}
	}

	return 0;
}

const char *get_modifier_name_by_mask(uint32_t modifier) {
	int i;
	for (i = 0; i < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++i) {
		if (modifiers[i].mod == modifier) {
			return modifiers[i].name;
		}
	}

	return NULL;
}

int get_modifier_names(const char **names, uint32_t modifier_masks) {
	int length = 0;
	int i;
	for (i = 0; i < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++i) {
		if ((modifier_masks & modifiers[i].mod) != 0) {
			names[length] = modifiers[i].name;
			++length;
			modifier_masks ^= modifiers[i].mod;
		}
	}

	return length;
}

pid_t get_parent_pid(pid_t child) {
	pid_t parent = -1;
	char file_name[100];
	char *buffer = NULL;
	char *token = NULL;
	const char *sep = " ";
	FILE *stat = NULL;

	sprintf(file_name, "/proc/%d/stat", child);

	if ((stat = fopen(file_name, "r"))) {
		if ((buffer = read_line(stat))) {
			token = strtok(buffer, sep); // pid
			token = strtok(NULL, sep);   // executable name
			token = strtok(NULL, sep);   // state
			token = strtok(NULL, sep);   // parent pid
			parent = strtol(token, NULL, 10);
		}
		free(buffer);
		fclose(stat);
	}

	if (parent) {
		return (parent == child) ? -1 : parent;
	}

	return -1;
}

uint32_t parse_color(const char *color) {
	if (color[0] == '#') {
		++color;
	}

	int len = strlen(color);
	if (len != 6 && len != 8) {
		wlr_log(WLR_DEBUG, "Invalid color %s, defaulting to color 0xFFFFFFFF", color);
		return 0xFFFFFFFF;
	}
	uint32_t res = (uint32_t)strtoul(color, NULL, 16);
	if (strlen(color) == 6) {
		res = (res << 8) | 0xFF;
	}
	return res;
}

bool parse_boolean(const char *boolean, const bool current) {
	if (strcmp(boolean, "1") == 0
			|| strcmp(boolean, "yes") == 0
			|| strcmp(boolean, "on") == 0
			|| strcmp(boolean, "true") == 0
			|| strcmp(boolean, "enable") == 0
			|| strcmp(boolean, "enabled") == 0
			|| strcmp(boolean, "active") == 0) {
		return true;
	} else if (strcmp(boolean, "toggle") == 0) {
		return !current;
	}
	return false;
}

char* resolve_path(const char* path) {
	struct stat sb;
	ssize_t r;
	int i;
	char *current = NULL;
	char *resolved = NULL;

	if(!(current = strdup(path))) {
		return NULL;
	}
	for (i = 0; i < 16; ++i) {
		if (lstat(current, &sb) == -1) {
			goto failed;
		}
		if((sb.st_mode & S_IFMT) != S_IFLNK) {
			return current;
		}
		if (!(resolved = malloc(sb.st_size + 1))) {
			goto failed;
		}
		r = readlink(current, resolved, sb.st_size);
		if (r == -1 || r > sb.st_size) {
			goto failed;
		}
		resolved[r] = '\0';
		free(current);
		current = strdup(resolved);
		free(resolved);
		resolved = NULL;
	}

failed:
	free(resolved);
	free(current);
	return NULL;
}
