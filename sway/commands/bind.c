#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

int binding_order = 0;

struct cmd_results *cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
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
		xkb_keysym_t sym = xkb_keysym_from_name(split->items[i], XKB_KEYSYM_CASE_INSENSITIVE);
		if (!sym) {
			error = cmd_results_new(CMD_INVALID, "bindsym", "Unknown key '%s'", (char *)split->items[i]);
			free_sway_binding(binding);
			list_free(split);
			return error;
		}
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		*key = sym;
		list_add(binding->keys, key);
	}
	free_flat_list(split);

	struct sway_mode *mode = config->current_mode;
	int i = list_seq_find(mode->bindings, sway_binding_cmp_keys, binding);
	if (i > -1) {
		sway_log(L_DEBUG, "bindsym - '%s' already exists, overwriting", argv[0]);
		struct sway_binding *dup = mode->bindings->items[i];
		free_sway_binding(dup);
		list_del(mode->bindings, i);
	}
	binding->order = binding_order++;
	list_add(mode->bindings, binding);
	list_qsort(mode->bindings, sway_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_bindcode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindcode", EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
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
			error = cmd_results_new(CMD_INVALID, "bindcode", "Invalid keycode '%s'", (char *)split->items[i]);
			free_sway_binding(binding);
			list_free(split);
			return error;
		}
		xkb_keycode_t *key = malloc(sizeof(xkb_keycode_t));
		*key = keycode - 8;
		list_add(binding->keys, key);
	}
	free_flat_list(split);

	struct sway_mode *mode = config->current_mode;
	int i = list_seq_find(mode->bindings, sway_binding_cmp_keys, binding);
	if (i > -1) {
		struct sway_binding *dup = mode->bindings->items[i];
		if (dup->bindcode) {
			sway_log(L_DEBUG, "bindcode - '%s' already exists, overwriting", argv[0]);
		} else {
			sway_log(L_DEBUG, "bindcode - '%s' already exists as bindsym, overwriting", argv[0]);
		}
		free_sway_binding(dup);
		list_del(mode->bindings, i);
	}
	binding->order = binding_order++;
	list_add(mode->bindings, binding);
	list_qsort(mode->bindings, sway_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindcode - Bound %s to command %s", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
