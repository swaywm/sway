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
		sway_log(L_DEBUG, "Invalid color %s, defaulting to color 0xFFFFFFFF", color);
		return 0xFFFFFFFF;
	}
	uint32_t res = (uint32_t)strtoul(color, NULL, 16);
	if (strlen(color) == 6) {
		res = (res << 8) | 0xFF;
	}
	return res;
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
