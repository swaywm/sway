#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include <wlr/util/log.h>

enum scaling_mode {
	SCALING_MODE_STRETCH,
	SCALING_MODE_FILL,
	SCALING_MODE_FIT,
	SCALING_MODE_CENTER,
	SCALING_MODE_TILE,
};

bool is_valid_color(const char *color) {
	int len = strlen(color);
	if (len != 7 || color[0] != '#') {
		wlr_log(L_ERROR, "%s is not a valid color for swaybg. "
				"Color should be specified as #rrggbb (no alpha).", color);
		return false;
	}

	int i;
	for (i = 1; i < len; ++i) {
		if (!isxdigit(color[i])) {
			return false;
		}
	}

	return true;
}

int main(int argc, const char **argv) {
	wlr_log_init(L_DEBUG, NULL);
	return 0;
}
