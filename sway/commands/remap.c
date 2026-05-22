#include <string.h>
#include <strings.h>
#include <xkbcommon/xkbcommon.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/keyboard.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

// Parse a key combination string like "Super-Shift-c" into modifiers and keysym
static bool parse_key_combo(const char *combo_str, uint32_t *modifiers, xkb_keysym_t *keysym) {
	*modifiers = 0;
	*keysym = XKB_KEY_NoSymbol;

	list_t *split = split_string(combo_str, "-");
	if (!split || split->length == 0) {
		list_free_items_and_destroy(split);
		return false;
	}

	// Parse all but last component as modifiers
	for (int i = 0; i < split->length - 1; i++) {
		char *mod_name = split->items[i];
		uint32_t mod = get_modifier_mask_by_name(mod_name);
		if (mod == 0) {
			// Special case for "C" and "Ctrl" shorthand
			if (strcasecmp(mod_name, "C") == 0 || strcasecmp(mod_name, "Ctrl") == 0) {
				mod = WLR_MODIFIER_CTRL;
			} else {
				list_free_items_and_destroy(split);
				return false;
			}
		}
		*modifiers |= mod;
	}

	// Last component is the key
	char *key_name = split->items[split->length - 1];
	*keysym = xkb_keysym_from_name(key_name, XKB_KEYSYM_CASE_INSENSITIVE);

	list_free_items_and_destroy(split);

	if (*keysym == XKB_KEY_NoSymbol) {
		return false;
	}

	return true;
}

// Implements the 'remap' command
// Syntax: remap <from-combo> <to-combo> [for <app_id>]
// Example: remap Super-c C-c for firefox
struct cmd_results *cmd_remap(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "remap", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	const char *from_str = argv[0];
	const char *to_str = argv[1];
	const char *app_id = NULL;

	if (argc >= 4 && strcasecmp(argv[2], "for") == 0) {
		app_id = argv[3];
	}

	struct sway_key_remap *remap = calloc(1, sizeof(struct sway_key_remap));
	if (!remap) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate key remap");
	}

	if (!parse_key_combo(from_str, &remap->from_modifiers, &remap->from_keysym)) {
		free(remap);
		return cmd_results_new(CMD_FAILURE, "Invalid 'from' key combination: %s", from_str);
	}

	if (!parse_key_combo(to_str, &remap->to_modifiers, &remap->to_keysym)) {
		free(remap);
		return cmd_results_new(CMD_FAILURE, "Invalid 'to' key combination: %s", to_str);
	}

	if (app_id) {
		remap->app_id = strdup(app_id);
		// App-specific remaps go at the front (higher priority)
		list_insert(config->key_remaps, 0, remap);
	} else {
		remap->app_id = NULL;
		// Global remaps go at the back (lower priority)
		list_add(config->key_remaps, remap);
	}

	sway_log(SWAY_DEBUG, "Added key remap: 0x%x+0x%x -> 0x%x+0x%x%s%s",
		remap->from_modifiers, remap->from_keysym,
		remap->to_modifiers, remap->to_keysym,
		app_id ? " for " : "", app_id ? app_id : "");

	return cmd_results_new(CMD_SUCCESS, NULL);
}


