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

	free(binding->keys);
	free(binding->command);
	free(binding);
}

static void *replace_binding(void *old_item, void *new_item) {
	struct sway_binding *old_binding = (struct sway_binding *)old_item;
	struct sway_binding *new_binding = (struct sway_binding *)new_item;
	wlr_log(WLR_DEBUG, "overwriting old binding command '%s' with command '%s'",
		old_binding->command, new_binding->command);
	free_sway_binding(old_binding);
	return new_item;
}

/**
 * Comparison function for bindings.
 *
 * Returns -1 if a < b, 0 if a == b, and 1 is a > b
 */
int sway_binding_cmp(const void *a, const void *b) {
	const struct sway_binding *binding_a = (const struct sway_binding *)a;
	const struct sway_binding *binding_b = (const struct sway_binding *)b;

	if (binding_a->release != binding_b->release) {
		return binding_a->release < binding_b->release ? -1 : 1;
	}

	if (binding_a->modifiers != binding_b->modifiers) {
		return binding_a->modifiers < binding_b->modifiers ? -1 : 1;
	}

	if (binding_a->length != binding_b->length) {
		return binding_a->length < binding_b->length ? -1 : 1;
	}

	// Keys are sorted
	for (size_t i = 0; i < binding_a->length; ++i) {
		if (binding_a->keys[i] != binding_b->keys[i]) {
			return binding_a->keys[i] < binding_b->keys[i] ? -1 : 1;
		}
	}

	return 0;
}

static int key_qsort_cmp(const void *pkey_a, const void *pkey_b) {
	uint32_t a = *(uint32_t *)pkey_a;
	uint32_t b = *(uint32_t *)pkey_b;
	return (a < b) ? -1 : ((a > b) ? 1 : 0);
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
	binding->keys = NULL;
	binding->length = 0;
	binding->modifiers = 0;
	binding->release = false;
	binding->locked = false;

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
	int nonmodifiers = 0;
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		if ((mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			binding->modifiers |= mod;
			continue;
		} else {
			nonmodifiers++;
		}
	}
	binding->keys = (uint32_t *)calloc(nonmodifiers, sizeof (uint32_t));
	if (!binding->keys) {
		return cmd_results_new(CMD_FAILURE, bindtype,
				"Unable to allocate binding key list");
	}
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		if (get_modifier_mask_by_name(split->items[i]) > 0) {
			continue;
		}

		if (bindcode) {
			// parse keycode
			xkb_keycode_t keycode = strtol(split->items[i], NULL, 10);
			if (!xkb_keycode_is_legal_ext(keycode)) {
				error =
					cmd_results_new(CMD_INVALID, "bindcode",
						"Invalid keycode '%s'", (char *)split->items[i]);
				free_sway_binding(binding);
				list_free(split);
				return error;
			}
			binding->keys[binding->length++] = (uint32_t)keycode;
		} else {
			// Check for xkb key
			xkb_keysym_t keysym = xkb_keysym_from_name(split->items[i],
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
			binding->keys[binding->length++] = (uint32_t)keysym;
		}
	}
	free_flat_list(split);
	binding->order = binding_order++;

	// sort ascending
	qsort(binding->keys, binding->length, sizeof(uint32_t), key_qsort_cmp);

	list_t *mode_bindings;
	if (bindcode) {
		mode_bindings = config->current_mode->keycode_bindings;
	} else {
		mode_bindings = config->current_mode->keysym_bindings;
	}

	list_sortedset_insert(mode_bindings, binding, sway_binding_cmp, replace_binding);

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
