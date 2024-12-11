#include "list.h"
#include "sway/commands.h"
#include "sway/criteria.h"
#include "sway/input/keyboard.h"
#include "sway/input/seat.h"
#include "sway/tree/container.h"
#include "stringop.h"
#include "util.h"


struct keycode_matches {
	xkb_keysym_t keysym;
	xkb_keycode_t keycode;
	int count;
};

static void find_keycode(struct xkb_keymap *keymap,
		xkb_keycode_t keycode, void *data) {
	xkb_keysym_t keysym = xkb_state_key_get_one_sym(
			config->keysym_translation_state, keycode);

	if (keysym == XKB_KEY_NoSymbol) {
		return;
	}

	struct keycode_matches *matches = data;
	if (matches->keysym == keysym) {
		matches->keycode = keycode;
		matches->count++;
	}
}

static struct keycode_matches get_keycode_for_keysym(xkb_keysym_t keysym) {
	struct keycode_matches matches = {
		.keysym = keysym,
		.keycode = XKB_KEYCODE_INVALID,
		.count = 0,
	};

	xkb_keymap_key_for_each(
			xkb_state_get_keymap(config->keysym_translation_state),
			find_keycode, &matches);
	return matches;
}

enum emit_action {
  EMIT_ACTION_RELEASE = 1 << 0,
  EMIT_ACTION_PRESS = 1 << 1
};


struct cmd_results *cmd_emit(int argc, char **argv) {
  struct cmd_results *error = NULL;
  if((error = checkarg(argc, "emit", EXPECTED_EQUAL_TO, 3))) {
    return error;
  }

  enum emit_action action = 0;
  if(strcmp("press", argv[0]) == 0) {
    action = EMIT_ACTION_PRESS;
  } else if(strcmp("release", argv[0]) == 0) {
    action = EMIT_ACTION_RELEASE;
  } else if(strcmp("press-release", argv[0]) == 0) {
    action = EMIT_ACTION_PRESS | EMIT_ACTION_RELEASE;
  }

  if(!action) {
    return cmd_results_new(CMD_INVALID, "Unknown emit action '%s'", argv[0]);
  }
  

  char *err_str = NULL;
  struct criteria *criteria = criteria_parse(argv[1], &err_str);
  if(!criteria) {
    error = cmd_results_new(CMD_INVALID, "%s", err_str);
    free(err_str);
    return error;
  }

  list_t *keycodes = create_list();
  list_t *split = split_string(argv[2], "+");

  uint32_t raw_modifiers = 0;
  for(int i = 0; i < split->length; i++) {
    uint32_t current_mod;

    // parse modifier key
    if((current_mod = get_modifier_mask_by_name(split->items[i])) > 0) {
      raw_modifiers |= current_mod;
      continue;
    }

    xkb_keysym_t keysym = xkb_keysym_from_name(split->items[i],
                                               XKB_KEYSYM_CASE_INSENSITIVE);
    if(!keysym) {
      criteria_destroy(criteria);
      list_free_items_and_destroy(keycodes);
      error = cmd_results_new(CMD_FAILURE,
                              "Unknown key '%s'", (char*) split->items[i]);
      list_free_items_and_destroy(split);
      return error;
    }

    struct keycode_matches matches = get_keycode_for_keysym(keysym);
    if(matches.count == 0) {
      criteria_destroy(criteria);
      list_free_items_and_destroy(keycodes);
      list_free_items_and_destroy(split);
      return cmd_results_new(CMD_FAILURE, "Unable to convert key to keycode");
    }
    
    uint32_t *keycode = malloc(sizeof(uint32_t));
    if(!keycode) {
      criteria_destroy(criteria);
      list_free_items_and_destroy(keycodes);
      list_free_items_and_destroy(split);
      return cmd_results_new(CMD_FAILURE, "Unable to allocate key");
    }
    *keycode = matches.keycode;
    list_add(keycodes, keycode);
  }

 
  list_t *containers = criteria_get_containers(criteria);

  struct sway_seat *seat = config->handler_context.seat;
  struct sway_container *original_container = seat_get_focused_container(seat);

  if(containers->length == 0) {
    return cmd_results_new(CMD_FAILURE, "no matching container found: %s", argv[1]);
  }
  struct sway_container *container = containers->items[0];
  bool is_same_surface = container->view->surface == original_container->view->surface;

  if(!is_same_surface) {
    wlr_seat_keyboard_enter(seat->wlr_seat, container->view->surface, 0, 0, NULL);
  }

  struct wlr_keyboard_modifiers wlr_modifiers = {
    .depressed = raw_modifiers
  };
  wlr_seat_keyboard_send_modifiers(seat->wlr_seat, &wlr_modifiers);

  // send key presses
  if(action & EMIT_ACTION_PRESS) {
    for(int i = 0; i < keycodes->length; i++) {
      wlr_seat_keyboard_notify_key(seat->wlr_seat, get_current_time_msec(), *((uint32_t*) keycodes->items[i])-8, 1);
    }
  }

  // send key releases
  if(action & EMIT_ACTION_RELEASE) {
    for(int i = 0; i < keycodes->length; i++) {
      wlr_seat_keyboard_notify_key(seat->wlr_seat, get_current_time_msec(), *((uint32_t*) keycodes->items[i])-8, 0);
    }
  }

  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

  if(is_same_surface) {
    wlr_seat_keyboard_send_modifiers(seat->wlr_seat, &keyboard->modifiers);
  } else {
    wlr_seat_keyboard_enter(seat->wlr_seat, original_container->view->surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
  }

  list_free(containers);

  criteria_destroy(criteria);

  return cmd_results_new(CMD_SUCCESS, NULL);
}
