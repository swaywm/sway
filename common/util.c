#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "readline.h"
#include "util.h"
#include "log.h"

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
	{ XKB_MOD_NAME_SHIFT, WLC_BIT_MOD_SHIFT },
	{ XKB_MOD_NAME_CAPS, WLC_BIT_MOD_CAPS },
	{ XKB_MOD_NAME_CTRL, WLC_BIT_MOD_CTRL },
	{ "Ctrl", WLC_BIT_MOD_CTRL },
	{ XKB_MOD_NAME_ALT, WLC_BIT_MOD_ALT },
	{ "Alt", WLC_BIT_MOD_ALT },
	{ XKB_MOD_NAME_NUM, WLC_BIT_MOD_MOD2 },
	{ "Mod3", WLC_BIT_MOD_MOD3 },
	{ XKB_MOD_NAME_LOGO, WLC_BIT_MOD_LOGO },
	{ "Mod5", WLC_BIT_MOD_MOD5 },
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
	pid_t parent;
	char file_name[100];
	char *buffer = NULL;
	char *token = NULL;
	const char sep[2] = " ";
	FILE *stat = NULL;

	sway_log(L_DEBUG, "trying to get parent pid for child pid %d", child);

	sprintf(file_name, "/proc/%d/stat", child);

	if (!(stat = fopen(file_name, "r")) || !(buffer = read_line(stat))) {
		return -1;
	}

	fclose(stat);

	sway_log(L_DEBUG, "buffer string is %s", buffer);

	token = strtok(buffer, sep);

	for (int i = 0; i < 3; i++) {
		token = strtok(NULL, sep);
	}

	parent = strtol(token, NULL, 10);

	sway_log(L_DEBUG, "found parent pid %d for child pid %d", parent, child);

	return (parent == child) ? -1 : parent;
}
