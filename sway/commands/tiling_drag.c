#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

static const char expected_syntax[] = "Expected 'tiling_drag "
	"enable|disable|toggle|off|modifier|titlebar|modifier titlebar'";

static bool parse_style(const char *word, enum tiling_drag_mode *style) {
	if (strcasecmp(word, "modifier") == 0) {
		*style = TILING_DRAG_MODIFIER;
	} else if (strcasecmp(word, "titlebar") == 0) {
		*style = TILING_DRAG_TITLEBAR;
	} else {
		return false;
	}
	return true;
}

static bool word_in(const char *word, const char *const *list) {
	for (; *list; ++list) {
		if (strcasecmp(word, *list) == 0) {
			return true;
		}
	}
	return false;
}

static bool parse_single_word(const char *word, enum tiling_drag_mode *mode) {
	// The words parse_boolean() treats as true, and i3's words for off
	static const char *const on_words[] = {
		"1", "yes", "on", "true", "enable", "enabled", "active", NULL,
	};
	static const char *const off_words[] = {
		"0", "no", "off", "false", "disable", "inactive", NULL,
	};

	if (parse_style(word, mode)) {
		return true;
	}
	if (word_in(word, on_words)) {
		*mode = TILING_DRAG_MODIFIER_OR_TITLEBAR;
	} else if (word_in(word, off_words)) {
		*mode = TILING_DRAG_OFF;
	} else if (strcasecmp(word, "toggle") == 0) {
		*mode = config->tiling_drag == TILING_DRAG_OFF ?
			TILING_DRAG_MODIFIER_OR_TITLEBAR : TILING_DRAG_OFF;
	} else {
		return false;
	}
	return true;
}

struct cmd_results *cmd_tiling_drag(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tiling_drag", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "tiling_drag", EXPECTED_AT_MOST, 2))) {
		return error;
	}

	enum tiling_drag_mode mode;
	if (argc == 1) {
		if (!parse_single_word(argv[0], &mode)) {
			return cmd_results_new(CMD_INVALID, "%s", expected_syntax);
		}
	} else {
		// Both drag styles, in either order: 'modifier titlebar'
		enum tiling_drag_mode first, second;
		if (!parse_style(argv[0], &first) || !parse_style(argv[1], &second) ||
				first == second) {
			return cmd_results_new(CMD_INVALID, "%s", expected_syntax);
		}
		mode = first | second;
	}

	config->tiling_drag = mode;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
