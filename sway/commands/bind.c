#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

int binding_order = 0;

void free_sway_binding(struct sway_binding *binding) {
	if (!binding) {
		return;
	}

	if (binding->keys) {
		free_flat_list(binding->keys);
	}
	free(binding->command);
	free(binding);
}

/**
 * Returns true if the bindings have the same key and modifier combinations.
 * Note that keyboard layout is not considered, so the bindings might actually
 * not be equivalent on some layouts.
 */
bool binding_key_compare(struct sway_binding *binding_a,
		struct sway_binding *binding_b) {
	if (binding_a->release != binding_b->release) {
		return false;
	}

	if (binding_a->bindcode != binding_b->bindcode) {
		return false;
	}

	if (binding_a->modifiers ^ binding_b->modifiers) {
		return false;
	}

	if (binding_a->keys->length != binding_b->keys->length) {
		return false;
	}

	int keys_len = binding_a->keys->length;
	for (int i = 0; i < keys_len; ++i) {
		uint32_t key_a = *(uint32_t*)binding_a->keys->items[i];
		bool found = false;
		for (int j = 0; j < keys_len; ++j) {
			uint32_t key_b = *(uint32_t*)binding_b->keys->items[j];
			if (key_b == key_a) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}

	return true;
}

struct cmd_results *cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = calloc(1, sizeof(struct sway_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "bindsym",
				"Unable to allocate binding");
	}
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->release = false;
	binding->bindcode = false;

	// Handle --release
	if (strcmp("--release", argv[0]) == 0) {
		if (argc >= 3) {
			binding->release = true;
			argv++;
			argc--;
		} else {
			free_sway_binding(binding);
			return cmd_results_new(CMD_FAILURE, "bindsym",
				"Invalid bindsym command "
				"(expected more than 2 arguments, got %d)", argc);
		}
	}

	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		if ((mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			binding->modifiers |= mod;
			continue;
		}
		// Check for xkb key
		xkb_keysym_t sym = xkb_keysym_from_name(split->items[i],
				XKB_KEYSYM_CASE_INSENSITIVE);

		// Check for mouse binding
		if (strncasecmp(split->items[i], "button", strlen("button")) == 0 &&
				strlen(split->items[i]) == strlen("button0")) {
			sym = ((char *)split->items[i])[strlen("button")] - '1' + BTN_LEFT;
		}
		if (!sym) {
			struct cmd_results *ret = cmd_results_new(CMD_INVALID, "bindsym",
					"Unknown key '%s'", (char *)split->items[i]);
			free_sway_binding(binding);
			free_flat_list(split);
			return ret;
		}
		xkb_keysym_t *key = calloc(1, sizeof(xkb_keysym_t));
		if (!key) {
			free_sway_binding(binding);
			free_flat_list(split);
			return cmd_results_new(CMD_FAILURE, "bindsym",
					"Unable to allocate binding");
		}
		*key = sym;
		list_add(binding->keys, key);
	}
	free_flat_list(split);
	binding->order = binding_order++;

	list_t *mode_bindings = config->current_mode->keysym_bindings;

	// overwrite the binding if it already exists
	bool overwritten = false;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_binding *config_binding = mode_bindings->items[i];
		if (binding_key_compare(binding, config_binding)) {
			sway_log(L_DEBUG, "overwriting old binding with command '%s'",
				config_binding->command);
			free_sway_binding(config_binding);
			mode_bindings->items[i] = binding;
			overwritten = true;
		}
	}

	if (!overwritten) {
		list_add(mode_bindings, binding);
	}

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s",
		argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_bindcode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindcode", EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = calloc(1, sizeof(struct sway_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "bindsym",
				"Unable to allocate binding");
	}
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->release = false;
	binding->bindcode = true;

	// Handle --release
	if (strcmp("--release", argv[0]) == 0) {
		if (argc >= 3) {
			binding->release = true;
			argv++;
			argc--;
		} else {
			free_sway_binding(binding);
			return cmd_results_new(CMD_FAILURE, "bindcode",
				"Invalid bindcode command "
				"(expected more than 2 arguments, got %d)", argc);
		}
	}

	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		if ((mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			binding->modifiers |= mod;
			continue;
		}
		// parse keycode
		xkb_keycode_t keycode = (int)strtol(split->items[i], NULL, 10);
		if (!xkb_keycode_is_legal_ext(keycode)) {
			error =
				cmd_results_new(CMD_INVALID, "bindcode",
					"Invalid keycode '%s'", (char *)split->items[i]);
			free_sway_binding(binding);
			list_free(split);
			return error;
		}
		xkb_keycode_t *key = calloc(1, sizeof(xkb_keycode_t));
		*key = keycode - 8;
		list_add(binding->keys, key);
	}
	free_flat_list(split);

	binding->order = binding_order++;

	list_t *mode_bindings = config->current_mode->keycode_bindings;

	// overwrite the binding if it already exists
	bool overwritten = false;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_binding *config_binding = mode_bindings->items[i];
		if (binding_key_compare(binding, config_binding)) {
			sway_log(L_DEBUG, "overwriting old binding with command '%s'",
				config_binding->command);
			free_sway_binding(config_binding);
			mode_bindings->items[i] = binding;
			overwritten = true;
		}
	}

	if (!overwritten) {
		list_add(mode_bindings, binding);
	}

	sway_log(L_DEBUG, "bindcode - Bound %s to command %s",
		argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
