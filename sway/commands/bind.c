#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input_state.h"
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
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "bindsym",
				"Unable to allocate binding");
	}
	binding->keys = list_new(sizeof(xkb_keysym_t *), 0);
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
	for (size_t i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		char *item = *(char **)list_get(split, i);
		if ((mod = get_modifier_mask_by_name(item)) > 0) {
			binding->modifiers |= mod;
			continue;
		}
		// Check for xkb key
		xkb_keysym_t sym = xkb_keysym_from_name(item,
				XKB_KEYSYM_CASE_INSENSITIVE);

		// Check for mouse binding
		if (strncasecmp(item, "button", strlen("button")) == 0 &&
				strlen(item) == strlen("button0")) {
			sym = ((char *)item)[strlen("button")] - '1' + M_LEFT_CLICK;
		}
		if (!sym) {
			free_sway_binding(binding);
			free_flat_list(split);
			return cmd_results_new(CMD_INVALID, "bindsym", "Unknown key '%s'",
					item);
		}
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		if (!key) {
			free_sway_binding(binding);
			free_flat_list(split);
			return cmd_results_new(CMD_FAILURE, "bindsym",
					"Unable to allocate binding");
		}
		*key = sym;
		list_add(binding->keys, &key);
	}
	free_flat_list(split);

	struct sway_mode *mode = config->current_mode;
	struct sway_binding *dup;
	ssize_t i = list_lsearch(mode->bindings, sway_binding_cmp_keys, binding, &dup);
	if (i > -1) {
		sway_log(L_DEBUG, "bindsym - '%s' already exists, overwriting", argv[0]);
		free_sway_binding(dup);
		list_delete(mode->bindings, i);
	}
	binding->order = binding_order++;
	list_add(mode->bindings, &binding);
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
	if (!binding) {
		return cmd_results_new(CMD_FAILURE, "bindsym",
				"Unable to allocate binding");
	}
	binding->keys = list_new(sizeof(xkb_keycode_t *), 0);
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
	for (size_t i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		char *item = *(char **)list_get(split, i);
		if ((mod = get_modifier_mask_by_name(item)) > 0) {
			binding->modifiers |= mod;
			continue;
		}
		// parse keycode
		xkb_keycode_t keycode = (int)strtol(item, NULL, 10);
		if (!xkb_keycode_is_legal_ext(keycode)) {
			error = cmd_results_new(CMD_INVALID, "bindcode", "Invalid keycode '%s'", item);
			free_sway_binding(binding);
			list_free(split);
			return error;
		}
		xkb_keycode_t *key = malloc(sizeof(xkb_keycode_t));
		*key = keycode - 8;
		list_add(binding->keys, &key);
	}
	free_flat_list(split);

	struct sway_mode *mode = config->current_mode;
	struct sway_binding *dup;
	ssize_t i = list_lsearch(mode->bindings, sway_binding_cmp_keys, binding, &dup);
	if (i > -1) {
		if (dup->bindcode) {
			sway_log(L_DEBUG, "bindcode - '%s' already exists, overwriting", argv[0]);
		} else {
			sway_log(L_DEBUG, "bindcode - '%s' already exists as bindsym, overwriting", argv[0]);
		}
		free_sway_binding(dup);
		list_delete(mode->bindings, i);
	}
	binding->order = binding_order++;
	list_add(mode->bindings, &binding);
	list_qsort(mode->bindings, sway_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindcode - Bound %s to command %s", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
