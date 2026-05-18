#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "sway/input/seat.h"
#include "sway/scene_descriptor.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/output.h"
#include "sway/input/text_input.h"
#include "sway/input/text_input_popup.h"
#include "sway/layers.h"
#include "sway/server.h"
#include <wlr/types/wlr_session_lock_v1.h>

enum sway_pending_im_event_type {
	SWAY_PENDING_IM_STATE,
	SWAY_PENDING_IM_KEY,
};

enum sway_pending_im_state_change {
	SWAY_PENDING_IM_STATE_UPDATE,
	SWAY_PENDING_IM_STATE_ACTIVATE,
	SWAY_PENDING_IM_STATE_DEACTIVATE,
};

struct sway_pending_im_event {
	struct wl_list link;
	enum sway_pending_im_event_type type;
};

struct sway_pending_im_state {
	struct sway_pending_im_event event;
	enum sway_pending_im_state_change change;
	uint32_t active_features;
	char *surrounding_text;
	uint32_t surrounding_cursor;
	uint32_t surrounding_anchor;
	uint32_t text_change_cause;
	uint32_t content_hint;
	uint32_t content_purpose;
};

struct sway_pending_im_key {
	struct sway_pending_im_event event;
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab;
	struct wlr_keyboard *keyboard;
	struct wl_listener keyboard_destroy;
	uint32_t time;
	uint32_t key;
	uint32_t state;
};

static struct sway_text_input *relay_get_focusable_text_input(
		struct sway_input_method_relay *relay) {
	struct sway_text_input *text_input = NULL;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		if (text_input->pending_focused_surface) {
			return text_input;
		}
	}
	return NULL;
}

static struct sway_text_input *relay_get_focused_text_input(
		struct sway_input_method_relay *relay) {
	struct sway_text_input *text_input = NULL;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		if (text_input->input->focused_surface) {
			return text_input;
		}
	}
	return NULL;
}

static void relay_process_pending_im_events(struct sway_input_method_relay *relay);
static void relay_clear_pending_im_events(struct sway_input_method_relay *relay);
static void relay_drop_pending_im_keys(struct sway_input_method_relay *relay);
static void relay_note_key_response(struct sway_input_method_relay *relay);

static void handle_im_commit(struct wl_listener *listener, void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_commit);

	struct sway_text_input *text_input = relay_get_focused_text_input(relay);
	if (!text_input) {
		relay_note_key_response(relay);
		return;
	}
	if (relay->input_method->current.preedit.text) {
		wlr_text_input_v3_send_preedit_string(text_input->input,
			relay->input_method->current.preedit.text,
			relay->input_method->current.preedit.cursor_begin,
			relay->input_method->current.preedit.cursor_end);
	}
	if (relay->input_method->current.commit_text) {
		wlr_text_input_v3_send_commit_string(text_input->input,
			relay->input_method->current.commit_text);
	}
	if (relay->input_method->current.delete.before_length
			|| relay->input_method->current.delete.after_length) {
		wlr_text_input_v3_send_delete_surrounding_text(text_input->input,
			relay->input_method->current.delete.before_length,
			relay->input_method->current.delete.after_length);
	}
	wlr_text_input_v3_send_done(text_input->input);
	relay_note_key_response(relay);
}

static void handle_im_keyboard_grab_destroy(struct wl_listener *listener, void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_keyboard_grab_destroy);
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = relay->input_method->keyboard_grab;
	struct wlr_seat *wlr_seat = keyboard_grab->input_method->seat;
	wl_list_remove(&relay->input_method_keyboard_grab_destroy.link);
	relay->pending_key_responses = 0;
	relay_drop_pending_im_keys(relay);
	relay_process_pending_im_events(relay);

	if (keyboard_grab->keyboard) {
		// send modifier state to original client
		wlr_seat_set_keyboard(wlr_seat, keyboard_grab->keyboard);
		wlr_seat_keyboard_notify_modifiers(wlr_seat,
			&keyboard_grab->keyboard->modifiers);
	}
}

static void handle_im_grab_keyboard(struct wl_listener *listener, void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_grab_keyboard);
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;

	// send modifier state to grab
	struct wlr_keyboard *active_keyboard = wlr_seat_get_keyboard(relay->seat->wlr_seat);
	wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab,
		active_keyboard);

	wl_signal_add(&keyboard_grab->events.destroy,
		&relay->input_method_keyboard_grab_destroy);
	relay->input_method_keyboard_grab_destroy.notify =
		handle_im_keyboard_grab_destroy;
}

static void text_input_set_pending_focused_surface(
		struct sway_text_input *text_input, struct wlr_surface *surface) {
	wl_list_remove(&text_input->pending_focused_surface_destroy.link);
	text_input->pending_focused_surface = surface;

	if (surface) {
		wl_signal_add(&surface->events.destroy,
			&text_input->pending_focused_surface_destroy);
	} else {
		wl_list_init(&text_input->pending_focused_surface_destroy.link);
	}
}

static void handle_im_destroy(struct wl_listener *listener, void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_destroy);
	wl_list_remove(&relay->input_method_commit.link);
	wl_list_remove(&relay->input_method_grab_keyboard.link);
	wl_list_remove(&relay->input_method_destroy.link);
	wl_list_remove(&relay->input_method_new_popup_surface.link);
	relay->input_method = NULL;
	relay->pending_key_responses = 0;
	relay_clear_pending_im_events(relay);
	struct sway_text_input *text_input = relay_get_focused_text_input(relay);
	if (text_input) {
		// keyboard focus is still there, so keep the surface at hand in case
		// the input method returns
		text_input_set_pending_focused_surface(text_input,
			text_input->input->focused_surface);
		wlr_text_input_v3_send_leave(text_input->input);
	}
}

static void constrain_popup(struct sway_input_popup *popup) {
	struct sway_text_input *text_input =
		relay_get_focused_text_input(popup->relay);

	if (!popup->desc.relative) {
		return;
	}

	struct wlr_box parent = {0};
	wlr_scene_node_coords(&popup->desc.relative->parent->node, &parent.x, &parent.y);

	struct wlr_box geo = {0};
	struct wlr_output *output;

	if (popup->desc.view) {
		struct sway_view *view = popup->desc.view;
		output = wlr_output_layout_output_at(root->output_layout,
			view->container->pending.content_x + view->geometry.x,
			view->container->pending.content_y + view->geometry.y);

		parent.width = view->geometry.width;
		parent.height = view->geometry.height;
		geo = view->geometry;
	} else {
		output = popup->fixed_output;
	}

	struct wlr_box output_box;
	wlr_output_layout_get_box(root->output_layout, output, &output_box);

	bool cursor_rect = text_input->input->current.features &
		WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE;
	struct wlr_box cursor_area;
	if (cursor_rect) {
		cursor_area = text_input->input->current.cursor_rectangle;
	} else {
		cursor_area = (struct wlr_box) {
			.width = parent.width,
			.height = parent.height,
		};
	}

	int popup_width = popup->popup_surface->surface->current.width;
	int popup_height = popup->popup_surface->surface->current.height;
	int x1 = parent.x + cursor_area.x;
	int x2 = parent.x + cursor_area.x + cursor_area.width;
	int y1 = parent.y + cursor_area.y;
	int y2 = parent.y + cursor_area.y + cursor_area.height;
	int x = x1;
	int y = y2;

	int available_right = output_box.x + output_box.width - x1;
	int available_left = x2 - output_box.x;
	if (available_right < popup_width && available_left > available_right) {
		x = x2 - popup_width;
	}

	int available_down = output_box.y + output_box.height - y2;
	int available_up = y1 - output_box.y;
	if (available_down < popup_height && available_up > available_down) {
		y = y1 - popup_height;
	}

	wlr_scene_node_set_position(popup->desc.relative, x - parent.x - geo.x, y - parent.y - geo.y);
	if (cursor_rect) {
		struct wlr_box box = {
			.x = x1 - x,
			.y = y1 - y,
			.width = cursor_area.width,
			.height = cursor_area.height,
		};
		wlr_input_popup_surface_v2_send_text_input_rectangle(
			popup->popup_surface, &box);
	}

	if (popup->scene_tree) {
		wlr_scene_node_set_position(&popup->scene_tree->node, x - geo.x, y - geo.y);
	}
}

static void input_popup_set_focus(struct sway_input_popup *popup,
		struct wlr_surface *surface);

static void relay_finish_pending_im_state(struct sway_pending_im_state *state) {
	free(state->surrounding_text);
}

static bool relay_init_pending_im_state(struct sway_pending_im_state *state,
		struct wlr_text_input_v3 *input,
		enum sway_pending_im_state_change change) {
	state->event.type = SWAY_PENDING_IM_STATE;
	state->change = change;
	state->active_features = input->active_features;
	state->surrounding_cursor = input->current.surrounding.cursor;
	state->surrounding_anchor = input->current.surrounding.anchor;
	state->text_change_cause = input->current.text_change_cause;
	state->content_hint = input->current.content_type.hint;
	state->content_purpose = input->current.content_type.purpose;

	if ((state->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) &&
			input->current.surrounding.text) {
		state->surrounding_text = strdup(input->current.surrounding.text);
		if (!state->surrounding_text) {
			return false;
		}
	}

	return true;
}

static void relay_send_im_state(struct sway_input_method_relay *relay,
		struct sway_pending_im_state *state) {
	struct wlr_input_method_v2 *input_method = relay->input_method;
	if (!input_method) {
		sway_log(SWAY_INFO, "Sending IM_DONE but im is gone");
		return;
	}

	if (state->change == SWAY_PENDING_IM_STATE_ACTIVATE) {
		wlr_input_method_v2_send_activate(input_method);
	} else if (state->change == SWAY_PENDING_IM_STATE_DEACTIVATE) {
		wlr_input_method_v2_send_deactivate(input_method);
	}

	// TODO: only send each of those if they were modified
	if (state->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
		wlr_input_method_v2_send_surrounding_text(input_method,
			state->surrounding_text ? state->surrounding_text : "",
			state->surrounding_cursor, state->surrounding_anchor);
	}
	wlr_input_method_v2_send_text_change_cause(input_method,
		state->text_change_cause);
	if (state->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
		wlr_input_method_v2_send_content_type(input_method,
			state->content_hint, state->content_purpose);
	}

	struct sway_text_input *text_input = relay_get_focused_text_input(relay);

	struct sway_input_popup *popup;
	wl_list_for_each(popup, &relay->input_popups, link) {
		if (text_input != NULL) {
			input_popup_set_focus(popup, text_input->input->focused_surface);
		} else {
			input_popup_set_focus(popup, NULL);
		}
	}
	wlr_input_method_v2_send_done(input_method);
	// TODO: pass intent, display popup size
}

static void relay_destroy_pending_im_event(struct sway_pending_im_event *event) {
	if (event->type == SWAY_PENDING_IM_STATE) {
		struct sway_pending_im_state *state =
			wl_container_of(event, state, event);
		relay_finish_pending_im_state(state);
		free(state);
	} else {
		struct sway_pending_im_key *key =
			wl_container_of(event, key, event);
		wl_list_remove(&key->keyboard_destroy.link);
		free(key);
	}
}

static void relay_clear_pending_im_events(struct sway_input_method_relay *relay) {
	struct sway_pending_im_event *event, *tmp;
	wl_list_for_each_safe(event, tmp, &relay->pending_im_events, link) {
		wl_list_remove(&event->link);
		relay_destroy_pending_im_event(event);
	}
}

static void relay_drop_pending_im_keys(struct sway_input_method_relay *relay) {
	struct sway_pending_im_event *event, *tmp;
	wl_list_for_each_safe(event, tmp, &relay->pending_im_events, link) {
		if (event->type != SWAY_PENDING_IM_KEY) {
			continue;
		}
		wl_list_remove(&event->link);
		relay_destroy_pending_im_event(event);
	}
}

static void relay_send_keyboard_grab_key(struct sway_input_method_relay *relay,
		struct wlr_input_method_keyboard_grab_v2 *keyboard_grab,
		struct wlr_keyboard *keyboard, uint32_t time, uint32_t key,
		uint32_t state) {
	if (!keyboard) {
		return;
	}

	wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab, keyboard);
	wlr_input_method_keyboard_grab_v2_send_key(keyboard_grab, time, key, state);
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		relay->pending_key_responses++;
	}
}

static void handle_pending_im_key_keyboard_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_pending_im_key *key =
		wl_container_of(listener, key, keyboard_destroy);
	wl_list_remove(&key->keyboard_destroy.link);
	wl_list_init(&key->keyboard_destroy.link);
	key->keyboard = NULL;
}

static bool relay_queue_keyboard_grab_key(struct sway_input_method_relay *relay,
		struct wlr_input_method_keyboard_grab_v2 *keyboard_grab,
		struct wlr_keyboard *keyboard, uint32_t time, uint32_t key,
		uint32_t state) {
	struct sway_pending_im_key *event = calloc(1, sizeof(*event));
	if (!event) {
		sway_log(SWAY_ERROR, "Unable to allocate pending input-method key");
		return false;
	}

	event->event.type = SWAY_PENDING_IM_KEY;
	event->keyboard_grab = keyboard_grab;
	event->keyboard = keyboard;
	event->time = time;
	event->key = key;
	event->state = state;
	wl_list_init(&event->keyboard_destroy.link);
	if (keyboard) {
		event->keyboard_destroy.notify = handle_pending_im_key_keyboard_destroy;
		wl_signal_add(&keyboard->base.events.destroy,
			&event->keyboard_destroy);
	}
	wl_list_insert(relay->pending_im_events.prev, &event->event.link);
	return true;
}

/*
 * zwp_input_method_v2.commit is serial-checked against the number of done
 * events Sway has already issued, but zwp_input_method_keyboard_grab_v2.key
 * has no matching response or ack serial. If Sway sends a new done after a key
 * has been forwarded to the input method, a later commit for that key can be
 * rejected as stale. Keep text-input state updates behind forwarded key presses
 * until the input method produces one of the responses Sway can observe
 * (input_method.commit or a same-client virtual-keyboard press), and keep later
 * key events behind any queued state so the input method sees the same order
 * Sway received.
 */
static void relay_process_pending_im_events(struct sway_input_method_relay *relay) {
	while (!wl_list_empty(&relay->pending_im_events)) {
		struct sway_pending_im_event *event =
			wl_container_of(relay->pending_im_events.next, event, link);
		if (event->type == SWAY_PENDING_IM_STATE) {
			if (relay->pending_key_responses > 0) {
				return;
			}

			struct sway_pending_im_state *state =
				wl_container_of(event, state, event);
			wl_list_remove(&event->link);
			relay_send_im_state(relay, state);
			relay_destroy_pending_im_event(event);
		} else {
			struct sway_pending_im_key *key =
				wl_container_of(event, key, event);
			wl_list_remove(&event->link);
			if (key->keyboard && relay->input_method &&
					relay->input_method->keyboard_grab == key->keyboard_grab) {
				relay_send_keyboard_grab_key(relay, key->keyboard_grab,
					key->keyboard, key->time, key->key, key->state);
			}
			relay_destroy_pending_im_event(event);
		}
	}
}

static void relay_queue_im_state(struct sway_input_method_relay *relay,
		struct wlr_text_input_v3 *input,
		enum sway_pending_im_state_change change) {
	if (wl_list_empty(&relay->pending_im_events) &&
			relay->pending_key_responses == 0) {
		struct sway_pending_im_state state = {0};
		if (!relay_init_pending_im_state(&state, input, change)) {
			sway_log(SWAY_ERROR, "Unable to allocate input-method state");
			return;
		}
		relay_send_im_state(relay, &state);
		relay_finish_pending_im_state(&state);
		return;
	}

	struct sway_pending_im_state *state = calloc(1, sizeof(*state));
	if (!state) {
		sway_log(SWAY_ERROR, "Unable to allocate pending input-method state");
		return;
	}
	if (!relay_init_pending_im_state(state, input, change)) {
		free(state);
		sway_log(SWAY_ERROR, "Unable to allocate pending input-method state");
		return;
	}

	wl_list_insert(relay->pending_im_events.prev, &state->event.link);
	relay_process_pending_im_events(relay);
}

static void relay_note_key_response(struct sway_input_method_relay *relay) {
	if (relay->pending_key_responses > 0) {
		relay->pending_key_responses--;
	}
	if (relay->pending_key_responses == 0) {
		relay_process_pending_im_events(relay);
	}
}

static void handle_text_input_enable(struct wl_listener *listener, void *data) {
	struct sway_text_input *text_input = wl_container_of(listener, text_input,
		text_input_enable);
	if (text_input->input->focused_surface == NULL) {
		sway_log(SWAY_DEBUG, "Enabling text input, but no longer focused");
		return;
	}
	if (text_input->relay->input_method == NULL) {
		sway_log(SWAY_INFO, "Enabling text input when input method is gone");
		return;
	}
	relay_queue_im_state(text_input->relay, text_input->input,
		SWAY_PENDING_IM_STATE_ACTIVATE);
}

static void handle_text_input_commit(struct wl_listener *listener,
		void *data) {
	struct sway_text_input *text_input = wl_container_of(listener, text_input,
		text_input_commit);
	if (text_input->input->focused_surface == NULL) {
		sway_log(SWAY_DEBUG, "Unfocused text input tried to commit an update");
		return;
	}
	if (!text_input->input->current_enabled) {
		sway_log(SWAY_INFO, "Inactive text input tried to commit an update");
		return;
	}
	sway_log(SWAY_DEBUG, "Text input committed update");
	if (text_input->relay->input_method == NULL) {
		sway_log(SWAY_INFO, "Text input committed, but input method is gone");
		return;
	}
	relay_queue_im_state(text_input->relay, text_input->input,
		SWAY_PENDING_IM_STATE_UPDATE);
}

static void relay_disable_text_input(struct sway_input_method_relay *relay,
		struct sway_text_input *text_input) {
	if (relay->input_method == NULL) {
		sway_log(SWAY_DEBUG, "Disabling text input, but input method is gone");
		return;
	}
	relay_queue_im_state(relay, text_input->input,
		SWAY_PENDING_IM_STATE_DEACTIVATE);
}

static void handle_text_input_disable(struct wl_listener *listener,
		void *data) {
	struct sway_text_input *text_input = wl_container_of(listener, text_input,
		text_input_disable);
	if (text_input->input->focused_surface == NULL) {
		sway_log(SWAY_DEBUG, "Disabling text input, but no longer focused");
		return;
	}
	relay_disable_text_input(text_input->relay, text_input);
}

static void handle_text_input_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_text_input *text_input = wl_container_of(listener, text_input,
		text_input_destroy);

	if (text_input->input->current_enabled) {
		relay_disable_text_input(text_input->relay, text_input);
	}
	text_input_set_pending_focused_surface(text_input, NULL);
	wl_list_remove(&text_input->text_input_commit.link);
	wl_list_remove(&text_input->text_input_destroy.link);
	wl_list_remove(&text_input->text_input_disable.link);
	wl_list_remove(&text_input->text_input_enable.link);
	wl_list_remove(&text_input->link);
	free(text_input);
}

static void handle_pending_focused_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_text_input *text_input = wl_container_of(listener, text_input,
		pending_focused_surface_destroy);
	text_input->pending_focused_surface = NULL;
	wl_list_remove(&text_input->pending_focused_surface_destroy.link);
	wl_list_init(&text_input->pending_focused_surface_destroy.link);
}

struct sway_text_input *sway_text_input_create(
		struct sway_input_method_relay *relay,
		struct wlr_text_input_v3 *text_input) {
	struct sway_text_input *input = calloc(1, sizeof(struct sway_text_input));
	if (!input) {
		return NULL;
	}
	input->input = text_input;
	input->relay = relay;

	wl_list_insert(&relay->text_inputs, &input->link);

	input->text_input_enable.notify = handle_text_input_enable;
	wl_signal_add(&text_input->events.enable, &input->text_input_enable);

	input->text_input_commit.notify = handle_text_input_commit;
	wl_signal_add(&text_input->events.commit, &input->text_input_commit);

	input->text_input_disable.notify = handle_text_input_disable;
	wl_signal_add(&text_input->events.disable, &input->text_input_disable);

	input->text_input_destroy.notify = handle_text_input_destroy;
	wl_signal_add(&text_input->events.destroy, &input->text_input_destroy);

	input->pending_focused_surface_destroy.notify =
		handle_pending_focused_surface_destroy;
	wl_list_init(&input->pending_focused_surface_destroy.link);
	return input;
}

static void relay_handle_text_input(struct wl_listener *listener,
		void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		text_input_new);
	struct wlr_text_input_v3 *wlr_text_input = data;
	if (relay->seat->wlr_seat != wlr_text_input->seat) {
		return;
	}

	sway_text_input_create(relay, wlr_text_input);
}

static void input_popup_set_focus(struct sway_input_popup *popup,
		struct wlr_surface *surface) {
	wl_list_remove(&popup->focused_surface_unmap.link);

	if (!popup->scene_tree) {
		wl_list_init(&popup->focused_surface_unmap.link);
		return;
	}

	if (popup->desc.relative) {
		scene_descriptor_destroy(&popup->scene_tree->node, SWAY_SCENE_DESC_POPUP);
		wlr_scene_node_destroy(popup->desc.relative);
		popup->desc.relative = NULL;
	}

	if (surface == NULL) {
		wl_list_init(&popup->focused_surface_unmap.link);
		wlr_scene_node_set_enabled(&popup->scene_tree->node, false);
		return;
	}

	struct wlr_layer_surface_v1 *layer_surface =
		wlr_layer_surface_v1_try_from_wlr_surface(surface);
	struct wlr_session_lock_surface_v1 *lock_surface =
		wlr_session_lock_surface_v1_try_from_wlr_surface(surface);

	struct wlr_scene_tree *relative_parent;
	if (layer_surface) {
		wl_signal_add(&layer_surface->surface->events.unmap,
			&popup->focused_surface_unmap);

		struct sway_layer_surface *layer = layer_surface->data;
		if (layer == NULL) {
			return;
		}

		relative_parent = layer->scene->tree;
		popup->desc.view = NULL;

		// we don't need to add an event here to NULL out this field because
		// this field will only be initialized if the popup is part of a layer
		// surface. Layer surfaces get destroyed as part of the output being
		// destroyed, thus also trickling down to popups.
		popup->fixed_output = layer->layer_surface->output;
	} else if (lock_surface) {
		wl_signal_add(&lock_surface->surface->events.unmap,
			&popup->focused_surface_unmap);

		struct sway_layer_surface *lock = lock_surface->data;
		if (lock == NULL) {
			return;
		}

		relative_parent = lock->scene->tree;
		popup->desc.view = NULL;

		// we don't need to add an event here to NULL out this field because
		// this field will only be initialized if the popup is part of a layer
		// surface. Layer surfaces get destroyed as part of the output being
		// destroyed, thus also trickling down to popups.
		popup->fixed_output = lock->layer_surface->output;
	} else {
		struct sway_view *view = view_from_wlr_surface(surface);
		// In the future there may be other shells been added, so we also need to check here.
		if (view == NULL) {
			sway_log(SWAY_DEBUG, "Unsupported IME focus surface");
			return;
		}
		wl_signal_add(&view->events.unmap, &popup->focused_surface_unmap);
		relative_parent = view->scene_tree;
		popup->desc.view = view;
	}

	struct wlr_scene_tree *relative = wlr_scene_tree_create(relative_parent);

	popup->desc.relative = &relative->node;
	if (!scene_descriptor_assign(&popup->scene_tree->node,
			SWAY_SCENE_DESC_POPUP, &popup->desc)) {
		wlr_scene_node_destroy(&popup->scene_tree->node);
		popup->scene_tree = NULL;
		return;
	}

	constrain_popup(popup);
	wlr_scene_node_set_enabled(&popup->scene_tree->node, true);
}

static void handle_im_popup_destroy(struct wl_listener *listener, void *data) {
	struct sway_input_popup *popup =
		wl_container_of(listener, popup, popup_destroy);
	wlr_scene_node_destroy(&popup->scene_tree->node);
	wl_list_remove(&popup->focused_surface_unmap.link);
	wl_list_remove(&popup->popup_surface_commit.link);
	wl_list_remove(&popup->popup_surface_map.link);
	wl_list_remove(&popup->popup_surface_unmap.link);
	wl_list_remove(&popup->popup_destroy.link);
	wl_list_remove(&popup->link);

	free(popup);
}

static void handle_im_popup_surface_map(struct wl_listener *listener, void *data) {
	struct sway_input_popup *popup =
		wl_container_of(listener, popup, popup_surface_map);
	struct sway_text_input *text_input = relay_get_focused_text_input(popup->relay);
	if (text_input != NULL) {
		input_popup_set_focus(popup, text_input->input->focused_surface);
	} else {
		input_popup_set_focus(popup, NULL);
	}
}

static void handle_im_popup_surface_unmap(struct wl_listener *listener, void *data) {
	struct sway_input_popup *popup =
		wl_container_of(listener, popup, popup_surface_unmap);

	scene_descriptor_destroy(&popup->scene_tree->node, SWAY_SCENE_DESC_POPUP);
	// relative should already be freed as it should be a child of the just unmapped scene
	popup->desc.relative = NULL;

	input_popup_set_focus(popup, NULL);
}

static void handle_im_popup_surface_commit(struct wl_listener *listener, void *data) {
	struct sway_input_popup *popup =
		wl_container_of(listener, popup, popup_surface_commit);

	constrain_popup(popup);
}

static void handle_im_focused_surface_unmap(
		struct wl_listener *listener, void *data) {
	struct sway_input_popup *popup =
		wl_container_of(listener, popup, focused_surface_unmap);

	input_popup_set_focus(popup, NULL);
}

static void handle_im_new_popup_surface(struct wl_listener *listener,
		void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_new_popup_surface);
	struct sway_input_popup *popup = calloc(1, sizeof(*popup));
	if (!popup) {
		sway_log(SWAY_ERROR, "Failed to allocate an input method popup");
		return;
	}

	popup->relay = relay;
	popup->popup_surface = data;
	popup->popup_surface->data = popup;

	popup->scene_tree = wlr_scene_tree_create(root->layers.popup);
	if (!popup->scene_tree) {
		sway_log(SWAY_ERROR, "Failed to allocate scene tree");
		free(popup);
		return;
	}

	if (!wlr_scene_subsurface_tree_create(popup->scene_tree,
			popup->popup_surface->surface)) {
		sway_log(SWAY_ERROR, "Failed to allocate subsurface tree");
		wlr_scene_node_destroy(&popup->scene_tree->node);
		free(popup);
		return;
	}

	wl_signal_add(&popup->popup_surface->events.destroy, &popup->popup_destroy);
	popup->popup_destroy.notify = handle_im_popup_destroy;
	wl_signal_add(&popup->popup_surface->surface->events.commit, &popup->popup_surface_commit);
	popup->popup_surface_commit.notify = handle_im_popup_surface_commit;
	wl_signal_add(&popup->popup_surface->surface->events.map, &popup->popup_surface_map);
	popup->popup_surface_map.notify = handle_im_popup_surface_map;
	wl_signal_add(&popup->popup_surface->surface->events.unmap, &popup->popup_surface_unmap);
	popup->popup_surface_unmap.notify = handle_im_popup_surface_unmap;
	wl_list_init(&popup->focused_surface_unmap.link);
	popup->focused_surface_unmap.notify = handle_im_focused_surface_unmap;

	struct sway_text_input *text_input = relay_get_focused_text_input(relay);
	if (text_input != NULL) {
		input_popup_set_focus(popup, text_input->input->focused_surface);
	} else {
		input_popup_set_focus(popup, NULL);
	}

	wl_list_insert(&relay->input_popups, &popup->link);
}

static void text_input_send_enter(struct sway_text_input *text_input,
		struct wlr_surface *surface) {
	wlr_text_input_v3_send_enter(text_input->input, surface);
	struct sway_input_popup *popup;
	wl_list_for_each(popup, &text_input->relay->input_popups, link) {
		input_popup_set_focus(popup, surface);
	}
}

static void relay_handle_input_method(struct wl_listener *listener,
		void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_new);
	struct wlr_input_method_v2 *input_method = data;
	if (relay->seat->wlr_seat != input_method->seat) {
		return;
	}

	if (relay->input_method != NULL) {
		sway_log(SWAY_INFO, "Attempted to connect second input method to a seat");
		wlr_input_method_v2_send_unavailable(input_method);
		return;
	}

	relay->input_method = input_method;
	wl_signal_add(&relay->input_method->events.commit,
		&relay->input_method_commit);
	relay->input_method_commit.notify = handle_im_commit;
	wl_signal_add(&relay->input_method->events.grab_keyboard,
		&relay->input_method_grab_keyboard);
	relay->input_method_grab_keyboard.notify = handle_im_grab_keyboard;
	wl_signal_add(&relay->input_method->events.destroy,
		&relay->input_method_destroy);
	relay->input_method_destroy.notify = handle_im_destroy;
	wl_signal_add(&relay->input_method->events.new_popup_surface,
		&relay->input_method_new_popup_surface);
	relay->input_method_new_popup_surface.notify = handle_im_new_popup_surface;

	struct sway_text_input *text_input = relay_get_focusable_text_input(relay);
	if (text_input) {
		text_input_send_enter(text_input,
			text_input->pending_focused_surface);
		text_input_set_pending_focused_surface(text_input, NULL);
	}
}

static void sway_input_method_relay_finish_text_input(struct sway_input_method_relay *relay) {
	wl_list_remove(&relay->text_input_new.link);
	wl_list_remove(&relay->text_input_manager_destroy.link);
	wl_list_init(&relay->text_input_new.link);
	wl_list_init(&relay->text_input_manager_destroy.link);
}

static void relay_handle_text_input_manager_destroy(struct wl_listener *listener, void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		text_input_manager_destroy);

	sway_input_method_relay_finish_text_input(relay);
}

static void sway_input_method_relay_finish_input_method(struct sway_input_method_relay *relay) {
	wl_list_remove(&relay->input_method_new.link);
	wl_list_remove(&relay->input_method_manager_destroy.link);
	wl_list_init(&relay->input_method_new.link);
	wl_list_init(&relay->input_method_manager_destroy.link);
}

static void relay_handle_input_method_manager_destroy(struct wl_listener *listener, void *data) {
	struct sway_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_manager_destroy);

	sway_input_method_relay_finish_input_method(relay);
}

void sway_input_method_relay_init(struct sway_seat *seat,
		struct sway_input_method_relay *relay) {
	relay->seat = seat;
	wl_list_init(&relay->text_inputs);
	wl_list_init(&relay->input_popups);
	wl_list_init(&relay->pending_im_events);

	relay->text_input_new.notify = relay_handle_text_input;
	wl_signal_add(&server.text_input->events.new_text_input,
		&relay->text_input_new);
	relay->text_input_manager_destroy.notify = relay_handle_text_input_manager_destroy;
	wl_signal_add(&server.text_input->events.destroy,
		&relay->text_input_manager_destroy);

	relay->input_method_new.notify = relay_handle_input_method;
	wl_signal_add(
		&server.input_method->events.new_input_method,
		&relay->input_method_new);
	relay->input_method_manager_destroy.notify = relay_handle_input_method_manager_destroy;
	wl_signal_add(&server.input_method->events.destroy,
		&relay->input_method_manager_destroy);
}

void sway_input_method_relay_finish(struct sway_input_method_relay *relay) {
	sway_input_method_relay_finish_text_input(relay);
	sway_input_method_relay_finish_input_method(relay);
	relay_clear_pending_im_events(relay);
}

void sway_input_method_relay_keyboard_grab_key(
		struct sway_input_method_relay *relay,
		struct wlr_input_method_keyboard_grab_v2 *keyboard_grab,
		struct wlr_keyboard *keyboard, uint32_t time, uint32_t key,
		uint32_t state) {
	if (!wl_list_empty(&relay->pending_im_events)) {
		relay_queue_keyboard_grab_key(relay, keyboard_grab, keyboard, time,
			key, state);
		relay_process_pending_im_events(relay);
		return;
	}

	relay_send_keyboard_grab_key(relay, keyboard_grab, keyboard, time, key,
		state);
}

void sway_input_method_relay_virtual_keyboard_key(
		struct sway_input_method_relay *relay,
		struct wlr_virtual_keyboard_v1 *virtual_keyboard, uint32_t state) {
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !virtual_keyboard ||
			!relay->input_method || !relay->input_method->keyboard_grab) {
		return;
	}

	if (wl_resource_get_client(virtual_keyboard->resource) ==
			wl_resource_get_client(relay->input_method->keyboard_grab->resource)) {
		// The input method forwarded the grabbed key instead of committing text.
		// This is the only observable response for the unhandled-key path.
		relay_note_key_response(relay);
	}
}

void sway_input_method_relay_set_focus(struct sway_input_method_relay *relay,
		struct wlr_surface *surface) {
	struct sway_text_input *text_input;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		if (text_input->pending_focused_surface) {
			assert(text_input->input->focused_surface == NULL);
			if (surface != text_input->pending_focused_surface) {
				text_input_set_pending_focused_surface(text_input, NULL);
			}
		} else if (text_input->input->focused_surface) {
			assert(text_input->pending_focused_surface == NULL);
			if (surface != text_input->input->focused_surface) {
				relay_disable_text_input(relay, text_input);
				wlr_text_input_v3_send_leave(text_input->input);
			} else {
				sway_log(SWAY_DEBUG, "IM relay set_focus already focused");
				continue;
			}
		}

		if (surface
				&& wl_resource_get_client(text_input->input->resource)
				== wl_resource_get_client(surface->resource)) {
			if (relay->input_method) {
				wlr_text_input_v3_send_enter(text_input->input, surface);
			} else {
				text_input_set_pending_focused_surface(text_input, surface);
			}
		}
	}
}
