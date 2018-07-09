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
static bool binding_key_compare(struct sway_binding *binding_a,
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

	// Keys are sorted
	int keys_len = binding_a->keys->length;
	for (int i = 0; i < keys_len; ++i) {
		uint32_t key_a = *(uint32_t *)binding_a->keys->items[i];
		uint32_t key_b = *(uint32_t *)binding_b->keys->items[i];
		if (key_a != key_b) {
			return false;
		}
	}

	return true;
}

static int key_qsort_cmp(const void *keyp_a, const void *keyp_b) {
	uint32_t key_a = **(uint32_t **)keyp_a;
	uint32_t key_b = **(uint32_t **)keyp_b;
	return (key_a < key_b) ? -1 : ((key_a > key_b) ? 1 : 0);
}

static struct cmd_results *cmd_bindsym_or_bindcode(int argc, char **argv,
		bool bindcode) {
	const char *bindtype = bindcode ? "bindcode" : "bindsym";

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, bindtype, EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = calloc(1, sizeof(struct sway_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, bindtype,
				"Unable to allocate binding");
	}
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->release = false;
	binding->locked = false;
	binding->bindcode = bindcode;

	// Handle --release and --locked
	while (argc > 0) {
		if (strcmp("--release", argv[0]) == 0) {
			binding->release = true;
		} else if (strcmp("--locked", argv[0]) == 0) {
			binding->locked = true;
		} else {
			break;
		}
		argv++;
		argc--;
	}
	if (argc < 2) {
		free_sway_binding(binding);
		return cmd_results_new(CMD_FAILURE, bindtype,
			"Invalid %s command "
			"(expected at least 2 non-option arguments, got %d)", bindtype, argc);
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

		xkb_keycode_t keycode;
		xkb_keysym_t keysym;
		if (bindcode) {
			// parse keycode
			keycode = (int)strtol(split->items[i], NULL, 10);
			if (!xkb_keycode_is_legal_ext(keycode)) {
				error =
					cmd_results_new(CMD_INVALID, "bindcode",
						"Invalid keycode '%s'", (char *)split->items[i]);
				free_sway_binding(binding);
				list_free(split);
				return error;
			}
		} else {
			// Check for xkb key
			 keysym = xkb_keysym_from_name(split->items[i],
					XKB_KEYSYM_CASE_INSENSITIVE);

			// Check for mouse binding
			if (strncasecmp(split->items[i], "button", strlen("button")) == 0 &&
					strlen(split->items[i]) == strlen("button0")) {
				keysym = ((char *)split->items[i])[strlen("button")] - '1' + BTN_LEFT;
			}
			if (!keysym) {
				struct cmd_results *ret = cmd_results_new(CMD_INVALID, "bindsym",
						"Unknown key '%s'", (char *)split->items[i]);
				free_sway_binding(binding);
				free_flat_list(split);
				return ret;
			}
		}
		uint32_t *key = calloc(1, sizeof(uint32_t));
		if (!key) {
			free_sway_binding(binding);
			free_flat_list(split);
			return cmd_results_new(CMD_FAILURE, bindtype,
					"Unable to allocate binding");
		}

		if (bindcode) {
			*key = (uint32_t)keycode;
		} else {
			*key = (uint32_t)keysym;
		}

		list_add(binding->keys, key);
	}
	free_flat_list(split);
	binding->order = binding_order++;

	// sort ascending
	list_qsort(binding->keys, key_qsort_cmp);

	list_t *mode_bindings;
	if (bindcode) {
		mode_bindings = config->current_mode->keycode_bindings;
	} else {
		mode_bindings = config->current_mode->keysym_bindings;
	}

	// overwrite the binding if it already exists
	bool overwritten = false;
	for (int i = 0; i < mode_bindings->length; ++i) {
		struct sway_binding *config_binding = mode_bindings->items[i];
		if (binding_key_compare(binding, config_binding)) {
			wlr_log(WLR_DEBUG, "overwriting old binding with command '%s'",
				config_binding->command);
			free_sway_binding(config_binding);
			mode_bindings->items[i] = binding;
			overwritten = true;
		}
	}

	if (!overwritten) {
		list_add(mode_bindings, binding);
	}

	wlr_log(WLR_DEBUG, "%s - Bound %s to command %s",
		bindtype, argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);

}

struct cmd_results *cmd_bindsym(int argc, char **argv) {
	return cmd_bindsym_or_bindcode(argc, argv, false);
}

struct cmd_results *cmd_bindcode(int argc, char **argv) {
	return cmd_bindsym_or_bindcode(argc, argv, true);
}
