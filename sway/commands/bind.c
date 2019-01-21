#define _POSIX_C_SOURCE 200809L
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/keyboard.h"
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

	list_free_items_and_destroy(binding->keys);
	free(binding->input);
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
	if (strcmp(binding_a->input, binding_b->input) != 0) {
		return false;
	}

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
	if (*type == BINDING_MOUSECODE) {
		// check for mouse bindcodes
		char *message = NULL;
		uint32_t button = get_mouse_bindcode(name, &message);
		if (!button) {
			if (message) {
				struct cmd_results *error =
					cmd_results_new(CMD_INVALID, message);
				free(message);
				return error;
			} else {
				return cmd_results_new(CMD_INVALID,
						"Unknown button code %s", name);
			}
		}
		*key_val = button;
	} else if (*type == BINDING_MOUSESYM) {
		// check for mouse bindsyms (x11 buttons or event names)
		char *message = NULL;
		uint32_t button = get_mouse_bindsym(name, &message);
		if (!button) {
			if (message) {
				struct cmd_results *error =
					cmd_results_new(CMD_INVALID, message);
				free(message);
				return error;
			} else if (!button) {
				return cmd_results_new(CMD_INVALID, "Unknown button %s", name);
			}
		}
		*key_val = button;
	} else if (*type == BINDING_KEYCODE) {
		// check for keycode. If it is the first key, allow mouse bindcodes
		if (first_key) {
			char *message = NULL;
			uint32_t button = get_mouse_bindcode(name, &message);
			free(message);
			if (button) {
				*type = BINDING_MOUSECODE;
				*key_val = button;
				return NULL;
			}
		}

		xkb_keycode_t keycode = strtol(name, NULL, 10);
		if (!xkb_keycode_is_legal_ext(keycode)) {
			if (first_key) {
				return cmd_results_new(CMD_INVALID,
						"Invalid keycode or button code '%s'", name);
			} else {
				return cmd_results_new(CMD_INVALID,
						"Invalid keycode '%s'", name);
			}
		}
		*key_val = keycode;
	} else {
		// check for keysym. If it is the first key, allow mouse bindsyms
		if (first_key) {
			char *message = NULL;
			uint32_t button = get_mouse_bindsym(name, &message);
			if (message) {
				struct cmd_results *error =
					cmd_results_new(CMD_INVALID, message);
				free(message);
				return error;
			} else if (button) {
				*type = BINDING_MOUSESYM;
				*key_val = button;
				return NULL;
			}
		}

		xkb_keysym_t keysym = xkb_keysym_from_name(name,
				XKB_KEYSYM_CASE_INSENSITIVE);
		if (!keysym) {
			if (first_key) {
				return cmd_results_new(CMD_INVALID,
						"Unknown key or button '%s'", name);
			} else {
				return cmd_results_new(CMD_INVALID, "Unknown key '%s'", name);
			}
		}
		*key_val = keysym;
	}
	return NULL;
}

static struct cmd_results *cmd_bindsym_or_bindcode(int argc, char **argv,
		bool bindcode) {
	const char *bindtype = bindcode ? "bindcode" : "bindsym";

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, bindtype, EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct sway_binding *binding = calloc(1, sizeof(struct sway_binding));
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate binding");
	}
	binding->input = strdup("*");
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->flags = 0;
	binding->type = bindcode ? BINDING_KEYCODE : BINDING_KEYSYM;

	bool exclude_titlebar = false;
	bool warn = true;

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
		} else if (strncmp("--input-device=", argv[0],
					strlen("--input-device=")) == 0) {
			free(binding->input);
			binding->input = strdup(argv[0] + strlen("--input-device="));
		} else if (strcmp("--no-warn", argv[0]) == 0) {
			warn = false;
		} else {
			break;
		}
		argv++;
		argc--;
	}
	if (binding->flags & (BINDING_BORDER | BINDING_CONTENTS | BINDING_TITLEBAR)
			|| exclude_titlebar) {
		binding->type = binding->type == BINDING_KEYCODE ?
			BINDING_MOUSECODE : BINDING_MOUSESYM;
	}

	if (argc < 2) {
		free_sway_binding(binding);
		return cmd_results_new(CMD_FAILURE,
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
			list_free_items_and_destroy(split);
			return cmd_results_new(CMD_FAILURE,
					"Unable to allocate binding key");
		}
		*key = key_val;
		list_add(binding->keys, key);
	}
	list_free_items_and_destroy(split);
	binding->order = binding_order++;

	// refine region of interest for mouse binding once we are certain
	// that this is one
	if (exclude_titlebar) {
		binding->flags &= ~BINDING_TITLEBAR;
	} else if (binding->type == BINDING_MOUSECODE
			|| binding->type == BINDING_MOUSESYM) {
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
			sway_log(SWAY_INFO, "Overwriting binding '%s' for device '%s' "
					"from `%s` to `%s`", argv[0], binding->input,
					binding->command, config_binding->command);
			if (warn) {
				config_add_swaynag_warning("Overwriting binding"
					"'%s' for device '%s' to `%s` from `%s`",
					argv[0], binding->input, binding->command,
					config_binding->command);
			}
			free_sway_binding(config_binding);
			mode_bindings->items[i] = binding;
			overwritten = true;
		}
	}

	if (!overwritten) {
		list_add(mode_bindings, binding);
	}

	sway_log(SWAY_DEBUG, "%s - Bound %s to command `%s` for device '%s'",
		bindtype, argv[0], binding->command, binding->input);
	return cmd_results_new(CMD_SUCCESS, NULL);
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
	sway_log(SWAY_DEBUG, "running command for binding: %s", binding->command);

	list_t *res_list = execute_command(binding->command, seat, NULL);
	bool success = true;
	for (int i = 0; i < res_list->length; ++i) {
		struct cmd_results *results = res_list->items[i];
		if (results->status != CMD_SUCCESS) {
			sway_log(SWAY_DEBUG, "could not run command for binding: %s (%s)",
				binding->command, results->error);
			success = false;
		}
		free_cmd_results(results);
	}
	list_free(res_list);
	if (success) {
		ipc_event_binding(binding);
	}
}
