#include "strings.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/keyboard.h"

struct cmd_results *cmd_resizing_corner(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "resizing_corner", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

    // Corresponds to WLR bitmasks (see edges.h)
    if (strcasecmp(argv[1], "all") == 0) {
      config->resizing_corner = WLR_EDGE_NONE;
    } else if (strcasecmp(argv[1], "topright") == 0) {
      config->resizing_corner = WLR_EDGE_TOP & WLR_EDGE_RIGHT;
    } else if (strcasecmp(argv[1], "bottomright") == 0) {
      config->resizing_corner = WLR_EDGE_BOTTOM & WLR_EDGE_RIGHT;
    } else if (strcasecmp(argv[1], "bottomleft") == 0) {
      config->resizing_corner = WLR_EDGE_BOTTOM & WLR_EDGE_LEFT;
    } else if (strcasecmp(argv[1], "topleft") == 0) {
      config->resizing_corner = WLR_EDGE_TOP & WLR_EDGE_LEFT;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Usage: resizing_corner [topright|bottomright|bottomleft|topleft|all]");
	}

	config->resizing_corner = corner;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
