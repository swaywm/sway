#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/keyboard.h"

static const char expected_syntax[] = "Expected 'tiling_drag "
	"enable|disable|toggle|off|modifier|titlebar|modifier titlebar' or "
	"'tiling_drag swap_modifier <modifier>[+<modifier>...]'";

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

// Parses a modifier name or a '+'-joined combination of names, as i3 spells
// them (e.g. 'Mod1+Shift'). Returns 0 if any part is not a modifier name.
static uint32_t parse_modifier_combination(const char *spec) {
	uint32_t mask = 0;
	const char *part = spec;
	while (true) {
		const char *end = strchr(part, '+');
		size_t len = end ? (size_t)(end - part) : strlen(part);
		char name[32];
		if (len == 0 || len >= sizeof(name)) {
			return 0;
		}
		memcpy(name, part, len);
		name[len] = '\0';
		uint32_t mod = get_modifier_mask_by_name(name);
		if (!mod) {
			return 0;
		}
		mask |= mod;
		if (!end) {
			return mask;
		}
		part = end + 1;
	}
}

static struct cmd_results *cmd_tiling_drag_swap_modifier(int argc,
		char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tiling_drag swap_modifier",
					EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	uint32_t mod = 0;
	if (strcasecmp(argv[0], "none") != 0) {
		mod = parse_modifier_combination(argv[0]);
		if (!mod) {
			return cmd_results_new(CMD_INVALID,
					"Invalid modifier '%s'", argv[0]);
		}
	}

	config->tiling_drag_swap_mod = mod;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_tiling_drag(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tiling_drag", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "swap_modifier") == 0) {
		return cmd_tiling_drag_swap_modifier(argc - 1, argv + 1);
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
