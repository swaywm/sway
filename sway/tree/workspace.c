#define _XOPEN_SOURCE 500
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "sway/container.h"
#include "log.h"

void next_name_map(swayc_t *ws, void *data) {
	int *count = data;
	++count;
}

char *workspace_next_name(const char *output_name) {
	wlr_log(L_DEBUG, "Workspace: Generating new workspace name for output %s",
			output_name);
	int count = 0;
	next_name_map(&root_container, &count);
	++count;
	int len = snprintf(NULL, 0, "%d", count);
	char *name = malloc(len + 1);
	if (!sway_assert(name, "Failed to allocate workspace name")) {
		return NULL;
	}
	snprintf(name, len + 1, "%d", count);
	return name;
}
