#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "container.h"
#include "focus.h"

struct cmd_results *cmd_border(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->active) {
		return cmd_results_new(CMD_FAILURE, "border", "Can only be used when sway is running.");
	}
	if ((error = checkarg(argc, "border", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (argc > 2) {
		return cmd_results_new(CMD_INVALID, "border",
			"Expected 'border <normal|pixel|none|toggle> [<n>]");
	}

	swayc_t *view = get_focused_view(&root_container);
	enum swayc_border_types border = view->border_type;
	int thickness = view->border_thickness;

	if (strcasecmp(argv[0], "none") == 0) {
		border = B_NONE;
	} else if (strcasecmp(argv[0], "normal") == 0) {
		border = B_NORMAL;
	} else if (strcasecmp(argv[0], "pixel") == 0) {
		border = B_PIXEL;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		switch (border) {
		case B_NONE:
			border = B_PIXEL;
			break;
		case B_NORMAL:
			border = B_NONE;
			break;
		case B_PIXEL:
			border = B_NORMAL;
			break;
		}
	} else {
		return cmd_results_new(CMD_INVALID, "border",
			"Expected 'border <normal|pixel|none|toggle>");
	}

	if (argc == 2 && (border == B_NORMAL || border == B_PIXEL)) {
		thickness = (int)strtol(argv[1], NULL, 10);
		if (errno == ERANGE || thickness < 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "border", "Number is out of range.");
		}
	}

	if (view) {
		view->border_type = border;
		view->border_thickness = thickness;
		update_geometry(view);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
