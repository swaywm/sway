#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "log.h"

static bool parse_layout_string(char *s, enum sway_container_layout *ptr) {
	if (strcasecmp(s, "splith") == 0) {
		*ptr = L_HORIZ;
	} else if (strcasecmp(s, "splitv") == 0) {
		*ptr = L_VERT;
	} else if (strcasecmp(s, "tabbed") == 0) {
		*ptr = L_TABBED;
	} else if (strcasecmp(s, "stacking") == 0) {
		*ptr = L_STACKED;
	} else {
		return false;
	}
	return true;
}

static const char* expected_syntax =
	"Expected 'layout default|tabbed|stacking|splitv|splith' or "
	"'layout toggle [split|all]' or "
	"'layout toggle [split|tabbed|stacking|splitv|splith] [split|tabbed|stacking|splitv|splith]...'";

struct cmd_results *cmd_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layout", EXPECTED_MORE_THAN, 0))) {
		return error;
	}
	struct sway_container *parent = config->handler_context.current_container;

	if (container_is_floating(parent)) {
		return cmd_results_new(CMD_FAILURE, "layout",
				"Unable to change layout of floating windows");
	}

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	if (strcasecmp(argv[0], "default") == 0) {
		parent->layout = parent->prev_layout;
	} else {
		if (parent->layout != L_TABBED && parent->layout != L_STACKED) {
			parent->prev_layout = parent->layout;
		}

		bool assigned_directly = parse_layout_string(argv[0], &parent->layout);
		if (!assigned_directly && strcasecmp(argv[0], "toggle") == 0) {
			if (argc == 1) {
				parent->layout =
					parent->layout == L_STACKED ? L_TABBED :
					parent->layout == L_TABBED ? parent->prev_layout : L_STACKED;
			} else if (argc == 2) {
				if (strcasecmp(argv[1], "all") == 0) {
					parent->layout =
						parent->layout == L_HORIZ ? L_VERT :
						parent->layout == L_VERT ? L_STACKED :
						parent->layout == L_STACKED ? L_TABBED : L_HORIZ;
				} else if (strcasecmp(argv[1], "split") == 0) {
					parent->layout = parent->layout == L_VERT ? L_HORIZ : L_VERT;
				} else {
					return cmd_results_new(CMD_INVALID, "layout", expected_syntax);
				}
			} else {
				bool valid;
				enum sway_container_layout parsed_layout;
				int curr = 1;
				for (; curr < argc; curr++) {
					valid = parse_layout_string(argv[curr], &parsed_layout);
					if (valid && parsed_layout == parent->layout) {
						break;
					}
				}
				for (int i = curr + 1; i != curr; ++i) {
					// cycle round to find next valid layout
					if (i >= argc) {
						i = 1;
					}
					if (parse_layout_string(argv[i], &parent->layout)) {
						break;
					} // invalid layout strings are silently ignored
				}
			}
		} else {
			return cmd_results_new(CMD_INVALID, "layout", expected_syntax);
		}
	}
	if (parent->layout == L_NONE) {
		parent->layout = container_get_default_layout(parent);
	}

	container_notify_subtree_changed(parent);
	arrange_windows(parent);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
