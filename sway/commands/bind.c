#define _XOPEN_SOURCE 500
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
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

static struct sway_binding *sway_binding_dup(struct sway_binding *sb) {
	struct sway_binding *new_sb = calloc(1, sizeof(struct sway_binding));
	if (!new_sb) {
		return NULL;
	}

	new_sb->type = sb->type;
	new_sb->order = sb->order;
	new_sb->flags = sb->flags;
	new_sb->modifiers = sb->modifiers;
	new_sb->command = strdup(sb->command);

	new_sb->keys = create_list();
	int i;
	for (i = 0; i < sb->keys->length; ++i) {
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		if (!key) {
			free_sway_binding(new_sb);
			return NULL;
		}
		*key = *(xkb_keysym_t *)sb->keys->items[i];
		list_add(new_sb->keys, key);
	}

	return new_sb;
}

/**
 * Returns true if the bindings have the same key and modifier combinations.
 * Note that keyboard layout is not considered, so the bindings might actually
 * not be equivalent on some layouts.
 */
static bool binding_key_compare(struct sway_binding *binding_a,
		struct sway_binding *binding_b) {
	if (binding_a->type != binding_b->type) {
		return false;
	}

	uint32_t conflict_generating_flags = BINDING_RELEASE | BINDING_BORDER
			| BINDING_CONTENTS | BINDING_TITLEBAR;
	if ((binding_a->flags & conflict_generating_flags) !=
			(binding_b->flags & conflict_generating_flags)) {
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


/**
 * From a keycode, bindcode, or bindsym name and the most likely binding type,
 * identify the appropriate numeric value corresponding to the key. Return NULL
 * and set *key_val if successful, otherwise return a specific error. Change
 * the value of *type if the initial type guess was incorrect and if this
 * was the first identified key.
 */
static struct cmd_results *identify_key(const char* name, bool first_key,
		uint32_t* key_val, enum binding_input_type* type) {
	if (*type == BINDING_KEYCODE) {
		// check for keycode
		xkb_keycode_t keycode = strtol(name, NULL, 10);
		if (!xkb_keycode_is_legal_ext(keycode)) {
			return cmd_results_new(CMD_INVALID, "bindcode",
					"Invalid keycode '%s'", name);
		}
		*key_val = keycode;
	} else {
		// check for keysym
		xkb_keysym_t keysym = xkb_keysym_from_name(name,
				XKB_KEYSYM_CASE_INSENSITIVE);

		// Check for mouse binding
		uint32_t button = 0;
		if (strncasecmp(name, "button", strlen("button")) == 0 &&
				strlen(name) == strlen("button0")) {
			button = name[strlen("button")] - '1' + BTN_LEFT;
		}

		if (*type == BINDING_KEYSYM) {
			if (button) {
				if (first_key) {
					*type = BINDING_MOUSE;
					*key_val = button;
				} else {
					return cmd_results_new(CMD_INVALID, "bindsym",
							"Mixed button '%s' into key sequence", name);
				}
			} else if (keysym) {
				*key_val = keysym;
			} else {
				return cmd_results_new(CMD_INVALID, "bindsym",
						"Unknown key '%s'", name);
			}
		} else {
			if (button) {
				*key_val = button;
			} else if (keysym) {
				return cmd_results_new(CMD_INVALID, "bindsym",
						"Mixed keysym '%s' into button sequence", name);
			} else {
				return cmd_results_new(CMD_INVALID, "bindsym",
						"Unknown button '%s'", name);
			}
		}
	}
	return NULL;
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
	binding->flags = 0;
	binding->type = bindcode ? BINDING_KEYCODE : BINDING_KEYSYM;

	bool exclude_titlebar = false;

	// Handle --release and --locked
	while (argc > 0) {
		if (strcmp("--release", argv[0]) == 0) {
			binding->flags |= BINDING_RELEASE;
		} else if (strcmp("--locked", argv[0]) == 0) {
			binding->flags |= BINDING_LOCKED;
		} else if (strcmp("--whole-window", argv[0]) == 0) {
			binding->flags |= BINDING_BORDER | BINDING_CONTENTS | BINDING_TITLEBAR;
		} else if (strcmp("--border", argv[0]) == 0) {
			binding->flags |= BINDING_BORDER;
		} else if (strcmp("--exclude-titlebar", argv[0]) == 0) {
			exclude_titlebar = true;
		} else {
			break;
		}
		argv++;
		argc--;
	}
	if (binding->flags & (BINDING_BORDER | BINDING_CONTENTS | BINDING_TITLEBAR)
			|| exclude_titlebar) {
		binding->type = BINDING_MOUSE;
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

		// Identify the key and possibly change binding->type
		uint32_t key_val = 0;
		error = identify_key(split->items[i], binding->keys->length == 0,
				     &key_val, &binding->type);
		if (error) {
			free_sway_binding(binding);
			list_free(split);
			return error;
		}

		uint32_t *key = calloc(1, sizeof(uint32_t));
		if (!key) {
			free_sway_binding(binding);
			free_flat_list(split);
			return cmd_results_new(CMD_FAILURE, bindtype,
					"Unable to allocate binding key");
		}
		*key = key_val;
		list_add(binding->keys, key);
	}
	free_flat_list(split);
	binding->order = binding_order++;

	// refine region of interest for mouse binding once we are certain
	// that this is one
	if (exclude_titlebar) {
		binding->flags &= ~BINDING_TITLEBAR;
	} else if (binding->type == BINDING_MOUSE) {
		binding->flags |= BINDING_TITLEBAR;
	}

	// sort ascending
	list_qsort(binding->keys, key_qsort_cmp);

	list_t *mode_bindings;
	if (binding->type == BINDING_KEYCODE) {
		mode_bindings = config->current_mode->keycode_bindings;
	} else if (binding->type == BINDING_KEYSYM) {
		mode_bindings = config->current_mode->keysym_bindings;
	} else {
		mode_bindings = config->current_mode->mouse_bindings;
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


/**
 * Execute the command associated to a binding
 */
void seat_execute_command(struct sway_seat *seat, struct sway_binding *binding) {
	wlr_log(WLR_DEBUG, "running command for binding: %s",
		binding->command);

	struct sway_binding *binding_copy = binding;
	bool reload = false;
	// if this is a reload command we need to make a duplicate of the
	// binding since it will be gone after the reload has completed.
	if (strcasecmp(binding->command, "reload") == 0) {
		reload = true;
		binding_copy = sway_binding_dup(binding);
		if (!binding_copy) {
			wlr_log(WLR_ERROR, "Failed to duplicate binding during reload");
			return;
		}
	}

	config->handler_context.seat = seat;
	struct cmd_results *results = execute_command(binding->command, NULL);
	if (results->status == CMD_SUCCESS) {
		ipc_event_binding(binding_copy);
	} else {
		wlr_log(WLR_DEBUG, "could not run command for binding: %s (%s)",
			binding->command, results->error);
	}

	if (reload) { // free the binding if we made a copy
		free_sway_binding(binding_copy);
	}
	free_cmd_results(results);
}
