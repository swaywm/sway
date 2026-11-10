#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <xkbcommon/xkbcommon.h>
#include "config.h"
#if HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif
#include "log.h"
#include "sway/a11y.h"
#include "sway/input/keyboard.h"

#define A11Y_BUS_NAME "org.freedesktop.a11y.Manager"
#define A11Y_OBJECT_PATH "/org/freedesktop/a11y/Manager"
#define A11Y_KEYBOARD_MONITOR_INTERFACE "org.freedesktop.a11y.KeyboardMonitor"
// Only the owner of this name may use the interface (as in mutter and KWin).
// It is the name Orca requests before calling anything.
#define A11Y_ALLOWED_NAME "org.gnome.Orca.KeyboardMonitor"

#define DBUS_NAME "org.freedesktop.DBus"
#define DBUS_PATH "/org/freedesktop/DBus"
#define DBUS_INTERFACE "org.freedesktop.DBus"

struct a11y_keystroke {
	uint32_t keysym;
	uint32_t modifiers;
};

struct a11y_client {
	struct wl_list link; // a11y_state::clients
	char *name; // unique bus name

	bool watch_all;
	bool grab_all;

	// keysyms used as the client's own modifiers (the Orca key)
	uint32_t *modifiers;
	size_t modifiers_len;
	struct a11y_keystroke *keystrokes;
	size_t keystrokes_len;

	// keys whose press was reported to this client, pending their release
	uint32_t reported_keys[WLR_KEYBOARD_KEYS_CAP];
	size_t reported_keys_len;
};

// A key whose press was grabbed: its release is withheld as well.
struct a11y_grabbed_key {
	uint32_t keycode;
	uint32_t keysym;
	// the press was the first tap of a grabbed modifier
	bool modifier_tap;
};

static struct a11y_state {
	bool initialized;
	sd_bus *bus;
	struct wl_event_source *bus_source;
	struct wl_event_source *timer_source;

	char *allowed_owner; // current unique owner of A11Y_ALLOWED_NAME
	struct wl_list clients; // a11y_client::link

	struct a11y_grabbed_key grabbed_keys[WLR_KEYBOARD_KEYS_CAP];
	size_t grabbed_keys_len;

	// The previous key event, to detect a double press of a grabbed modifier
	bool have_last;
	uint32_t last_keycode;
	bool last_pressed;
	uint32_t last_time_msec;
	// Set by the quick release of a grabbed modifier's first tap: a press of
	// the same key within the repeat delay is then handled normally.
	bool tap_armed;
	uint32_t tap_keycode;

	// Result for the event being handled, for its second pass through
	// handle_key_event() when the keyboard is part of a keyboard group.
	struct {
		bool valid;
		const struct wlr_keyboard_key_event *event;
		uint32_t keycode;
		uint32_t time_msec;
		enum wl_keyboard_key_state state;
		bool grabbed;
	} pending;
} a11y;

static void update_bus_sources(void);

static void client_destroy(struct a11y_client *client) {
	wl_list_remove(&client->link);
	free(client->name);
	free(client->modifiers);
	free(client->keystrokes);
	free(client);
}

static struct a11y_client *client_find(const char *name) {
	struct a11y_client *client;
	wl_list_for_each(client, &a11y.clients, link) {
		if (strcmp(client->name, name) == 0) {
			return client;
		}
	}
	return NULL;
}

static struct a11y_client *client_ensure(const char *name) {
	struct a11y_client *client = client_find(name);
	if (client) {
		return client;
	}
	client = calloc(1, sizeof(*client));
	if (!client) {
		return NULL;
	}
	client->name = strdup(name);
	if (!client->name) {
		free(client);
		return NULL;
	}
	wl_list_insert(a11y.clients.prev, &client->link);
	sway_log(SWAY_DEBUG, "a11y: new keyboard monitor client %s", name);
	return client;
}

static void destroy_all_clients(void) {
	struct a11y_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &a11y.clients, link) {
		client_destroy(client);
	}
}

static void set_allowed_owner(const char *owner) {
	free(a11y.allowed_owner);
	a11y.allowed_owner = owner && owner[0] ? strdup(owner) : NULL;
}

static void bus_disconnect(void) {
	if (a11y.bus_source) {
		wl_event_source_remove(a11y.bus_source);
		a11y.bus_source = NULL;
	}
	if (a11y.timer_source) {
		wl_event_source_remove(a11y.timer_source);
		a11y.timer_source = NULL;
	}
	if (a11y.bus) {
		sd_bus_close(a11y.bus);
		a11y.bus = sd_bus_unref(a11y.bus);
	}
	set_allowed_owner(NULL);
	if (a11y.initialized) {
		destroy_all_clients();
	}
}

// D-Bus methods

static int check_access(sd_bus_message *msg, sd_bus_error *error) {
	const char *sender = sd_bus_message_get_sender(msg);
	if (!sender || !a11y.allowed_owner ||
			strcmp(sender, a11y.allowed_owner) != 0) {
		return sd_bus_error_set(error, SD_BUS_ERROR_ACCESS_DENIED,
			"Only the owner of " A11Y_ALLOWED_NAME
			" may use this interface");
	}
	return 0;
}

static int get_client(sd_bus_message *msg, sd_bus_error *error,
		struct a11y_client **out) {
	int r = check_access(msg, error);
	if (r < 0) {
		return r;
	}
	*out = client_ensure(sd_bus_message_get_sender(msg));
	if (!*out) {
		return sd_bus_error_set_errno(error, ENOMEM);
	}
	return 0;
}

static int handle_grab_keyboard(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct a11y_client *client;
	int r = get_client(msg, error, &client);
	if (r < 0) {
		return r;
	}
	client->grab_all = true;
	return sd_bus_reply_method_return(msg, "");
}

static int handle_ungrab_keyboard(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	int r = check_access(msg, error);
	if (r < 0) {
		return r;
	}
	struct a11y_client *client = client_find(sd_bus_message_get_sender(msg));
	if (client) {
		client->grab_all = false;
	}
	return sd_bus_reply_method_return(msg, "");
}

static int handle_watch_keyboard(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct a11y_client *client;
	int r = get_client(msg, error, &client);
	if (r < 0) {
		return r;
	}
	client->watch_all = true;
	return sd_bus_reply_method_return(msg, "");
}

static int handle_unwatch_keyboard(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	int r = check_access(msg, error);
	if (r < 0) {
		return r;
	}
	struct a11y_client *client = client_find(sd_bus_message_get_sender(msg));
	if (client) {
		client->watch_all = false;
	}
	return sd_bus_reply_method_return(msg, "");
}

static int handle_set_key_grabs(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	int r = check_access(msg, error);
	if (r < 0) {
		return r;
	}

	const void *mods_data = NULL;
	size_t mods_size = 0;
	r = sd_bus_message_read_array(msg, 'u', &mods_data, &mods_size);
	if (r < 0) {
		return r;
	}
	size_t modifiers_len = mods_size / sizeof(uint32_t);
	uint32_t *modifiers = NULL;
	if (modifiers_len > 0) {
		modifiers = malloc(modifiers_len * sizeof(uint32_t));
		if (!modifiers) {
			return sd_bus_error_set_errno(error, ENOMEM);
		}
		memcpy(modifiers, mods_data, modifiers_len * sizeof(uint32_t));
	}

	struct a11y_keystroke *keystrokes = NULL;
	size_t keystrokes_len = 0, keystrokes_cap = 0;
	r = sd_bus_message_enter_container(msg, 'a', "(uu)");
	if (r < 0) {
		goto error;
	}
	while (true) {
		uint32_t keysym, mask;
		r = sd_bus_message_read(msg, "(uu)", &keysym, &mask);
		if (r < 0) {
			goto error;
		} else if (r == 0) {
			break;
		}
		if (keystrokes_len == keystrokes_cap) {
			size_t cap = keystrokes_cap ? keystrokes_cap * 2 : 8;
			struct a11y_keystroke *new_keystrokes =
				realloc(keystrokes, cap * sizeof(*keystrokes));
			if (!new_keystrokes) {
				r = sd_bus_error_set_errno(error, ENOMEM);
				goto error;
			}
			keystrokes = new_keystrokes;
			keystrokes_cap = cap;
		}
		keystrokes[keystrokes_len++] = (struct a11y_keystroke){
			.keysym = keysym,
			.modifiers = mask,
		};
	}
	r = sd_bus_message_exit_container(msg);
	if (r < 0) {
		goto error;
	}

	struct a11y_client *client = client_ensure(sd_bus_message_get_sender(msg));
	if (!client) {
		r = sd_bus_error_set_errno(error, ENOMEM);
		goto error;
	}
	// Replace whatever the client set before
	free(client->modifiers);
	free(client->keystrokes);
	client->modifiers = modifiers;
	client->modifiers_len = modifiers_len;
	client->keystrokes = keystrokes;
	client->keystrokes_len = keystrokes_len;
	return sd_bus_reply_method_return(msg, "");

error:
	free(modifiers);
	free(keystrokes);
	return r;
}

static const sd_bus_vtable keyboard_monitor_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("GrabKeyboard", "", "", handle_grab_keyboard,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("UngrabKeyboard", "", "", handle_ungrab_keyboard,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("WatchKeyboard", "", "", handle_watch_keyboard,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("UnwatchKeyboard", "", "", handle_unwatch_keyboard,
		SD_BUS_VTABLE_UNPRIVILEGED),
	// basu has no SD_BUS_METHOD_WITH_NAMES()/SD_BUS_SIGNAL_WITH_NAMES()
	SD_BUS_METHOD("SetKeyGrabs", "aua(uu)", "", handle_set_key_grabs,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("KeyEvent", "buuuq", 0),
	SD_BUS_VTABLE_END,
};

// Bus name tracking

static int handle_name_owner_changed(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	const char *name, *old_owner, *new_owner;
	int r = sd_bus_message_read(msg, "sss", &name, &old_owner, &new_owner);
	if (r < 0) {
		sway_log(SWAY_ERROR, "a11y: failed to parse NameOwnerChanged: %s",
			strerror(-r));
		return 0;
	}
	if (strcmp(name, A11Y_ALLOWED_NAME) == 0) {
		set_allowed_owner(new_owner);
	} else if (name[0] == ':' && new_owner[0] == '\0') {
		// A connection closed: its watch, grabs and key grabs end
		struct a11y_client *client = client_find(name);
		if (client) {
			sway_log(SWAY_DEBUG, "a11y: keyboard monitor client %s "
				"disconnected", name);
			client_destroy(client);
		}
	}
	return 0;
}

static int handle_get_allowed_owner(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	const char *owner = NULL;
	if (!sd_bus_message_is_method_error(msg, NULL)) {
		if (sd_bus_message_read(msg, "s", &owner) < 0) {
			owner = NULL;
		}
	}
	set_allowed_owner(owner);
	return 0;
}

static int handle_request_name(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	const sd_bus_error *err = sd_bus_message_get_error(msg);
	if (err) {
		sway_log(SWAY_ERROR, "a11y: failed to request " A11Y_BUS_NAME ": %s",
			err->message);
		return 0;
	}
	uint32_t ret = 0;
	if (sd_bus_message_read(msg, "u", &ret) >= 0 &&
			ret != 1 /* DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER */ &&
			ret != 4 /* DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER */) {
		sway_log(SWAY_ERROR, "a11y: " A11Y_BUS_NAME " is owned by "
			"another process");
		return 0;
	}
	sway_log(SWAY_DEBUG, "a11y: acquired " A11Y_BUS_NAME);
	return 0;
}

// Event loop integration

static void process_bus(void) {
	if (!a11y.bus) {
		return;
	}
	int r;
	do {
		r = sd_bus_process(a11y.bus, NULL);
	} while (r > 0);
	if (r < 0) {
		sway_log(SWAY_ERROR, "a11y: lost the session bus connection: %s",
			strerror(-r));
		bus_disconnect();
		return;
	}
	update_bus_sources();
}

static int handle_bus_fd(int fd, uint32_t mask, void *data) {
	process_bus();
	return 0;
}

static int handle_bus_timer(void *data) {
	process_bus();
	return 0;
}

static void update_bus_sources(void) {
	if (!a11y.bus) {
		return;
	}

	int events = sd_bus_get_events(a11y.bus);
	uint32_t mask = 0;
	if (events > 0 && (events & POLLIN)) {
		mask |= WL_EVENT_READABLE;
	}
	if (events > 0 && (events & POLLOUT)) {
		mask |= WL_EVENT_WRITABLE;
	}
	wl_event_source_fd_update(a11y.bus_source, mask);

	uint64_t usec = UINT64_MAX;
	int timeout_ms = 0; // disarmed
	if (sd_bus_get_timeout(a11y.bus, &usec) >= 0 && usec != UINT64_MAX) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		uint64_t now_usec = (uint64_t)now.tv_sec * 1000000 +
			(uint64_t)now.tv_nsec / 1000;
		if (usec <= now_usec) {
			timeout_ms = 1;
		} else {
			uint64_t ms = (usec - now_usec + 999) / 1000;
			timeout_ms = ms > INT_MAX ? INT_MAX : (int)ms;
		}
	}
	wl_event_source_timer_update(a11y.timer_source, timeout_ms);
}

void a11y_init(struct wl_event_loop *loop) {
	if (a11y.initialized) {
		return;
	}
	wl_list_init(&a11y.clients);
	a11y.initialized = true;

	int r = sd_bus_open_user(&a11y.bus);
	if (r < 0) {
		sway_log(SWAY_INFO, "a11y: no session bus (%s), "
			A11Y_KEYBOARD_MONITOR_INTERFACE " disabled", strerror(-r));
		a11y.bus = NULL;
		return;
	}

	r = sd_bus_add_object_vtable(a11y.bus, NULL, A11Y_OBJECT_PATH,
		A11Y_KEYBOARD_MONITOR_INTERFACE, keyboard_monitor_vtable, NULL);
	if (r < 0) {
		goto error;
	}
	// Follow the owner of the allowed name and the end of client
	// connections. The match is installed before GetNameOwner and the name
	// request are sent, so the bus delivers every later change after them.
	r = sd_bus_match_signal_async(a11y.bus, NULL, DBUS_NAME, DBUS_PATH,
		DBUS_INTERFACE, "NameOwnerChanged", handle_name_owner_changed,
		NULL, NULL);
	if (r < 0) {
		goto error;
	}
	r = sd_bus_call_method_async(a11y.bus, NULL, DBUS_NAME, DBUS_PATH,
		DBUS_INTERFACE, "GetNameOwner", handle_get_allowed_owner, NULL,
		"s", A11Y_ALLOWED_NAME);
	if (r < 0) {
		goto error;
	}
	r = sd_bus_request_name_async(a11y.bus, NULL, A11Y_BUS_NAME, 0,
		handle_request_name, NULL);
	if (r < 0) {
		goto error;
	}

	int fd = sd_bus_get_fd(a11y.bus);
	if (fd < 0) {
		r = fd;
		goto error;
	}
	a11y.bus_source = wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
		handle_bus_fd, NULL);
	a11y.timer_source = wl_event_loop_add_timer(loop, handle_bus_timer, NULL);
	if (!a11y.bus_source || !a11y.timer_source) {
		r = -ENOMEM;
		goto error;
	}
	update_bus_sources();
	sway_log(SWAY_DEBUG, "a11y: providing " A11Y_KEYBOARD_MONITOR_INTERFACE);
	return;

error:
	sway_log(SWAY_ERROR, "a11y: failed to set up "
		A11Y_KEYBOARD_MONITOR_INTERFACE ": %s", strerror(-r));
	bus_disconnect();
}

void a11y_finish(void) {
	if (!a11y.initialized) {
		return;
	}
	bus_disconnect();
	a11y.initialized = false;
}

// Key handling

static void emit_key_event(struct a11y_client *client, bool released,
		uint32_t state, uint32_t keysym, uint32_t unichar, uint32_t keycode) {
	if (!a11y.bus) {
		return;
	}
	sd_bus_message *msg = NULL;
	int r = sd_bus_message_new_signal(a11y.bus, &msg, A11Y_OBJECT_PATH,
		A11Y_KEYBOARD_MONITOR_INTERFACE, "KeyEvent");
	if (r >= 0) {
		// Sent to the one client it concerns, never broadcast
		r = sd_bus_message_set_destination(msg, client->name);
	}
	if (r >= 0) {
		r = sd_bus_message_append(msg, "buuuq", (int)released, state,
			keysym, unichar, (uint16_t)keycode);
	}
	if (r >= 0) {
		r = sd_bus_send(a11y.bus, msg, NULL);
	}
	if (r < 0) {
		sway_log(SWAY_ERROR, "a11y: failed to send KeyEvent to %s: %s",
			client->name, strerror(-r));
	}
	sd_bus_message_unref(msg);
}

static bool client_has_modifier(struct a11y_client *client, uint32_t keysym) {
	for (size_t i = 0; i < client->modifiers_len; i++) {
		if (client->modifiers[i] == keysym) {
			return true;
		}
	}
	return false;
}

static bool is_grabbed_modifier(uint32_t keysym) {
	struct a11y_client *client;
	wl_list_for_each(client, &a11y.clients, link) {
		if (client_has_modifier(client, keysym)) {
			return true;
		}
	}
	return false;
}

// Whether one of the client's modifiers is held down (its press grabbed)
static bool client_modifier_held(struct a11y_client *client,
		uint32_t except_keycode) {
	for (size_t i = 0; i < a11y.grabbed_keys_len; i++) {
		struct a11y_grabbed_key *key = &a11y.grabbed_keys[i];
		if (key->keycode != except_keycode &&
				client_has_modifier(client, key->keysym)) {
			return true;
		}
	}
	return false;
}

static bool client_grabs_key(struct a11y_client *client, uint32_t keycode,
		uint32_t keysym, uint32_t state) {
	if (client->grab_all || client_has_modifier(client, keysym) ||
			client_modifier_held(client, keycode)) {
		return true;
	}
	for (size_t i = 0; i < client->keystrokes_len; i++) {
		// The keysym and the whole modifier mask must match
		if (client->keystrokes[i].keysym == keysym &&
				client->keystrokes[i].modifiers == state) {
			return true;
		}
	}
	return false;
}

static bool client_watches_key(struct a11y_client *client, uint32_t keycode,
		uint32_t keysym, uint32_t state) {
	return client->watch_all ||
		client_grabs_key(client, keycode, keysym, state);
}

static void client_add_reported_key(struct a11y_client *client,
		uint32_t keycode) {
	if (client->reported_keys_len < WLR_KEYBOARD_KEYS_CAP) {
		client->reported_keys[client->reported_keys_len++] = keycode;
	}
}

static bool client_remove_reported_key(struct a11y_client *client,
		uint32_t keycode) {
	for (size_t i = 0; i < client->reported_keys_len; i++) {
		if (client->reported_keys[i] == keycode) {
			client->reported_keys[i] =
				client->reported_keys[--client->reported_keys_len];
			return true;
		}
	}
	return false;
}

static struct a11y_grabbed_key *find_grabbed_key(uint32_t keycode) {
	for (size_t i = 0; i < a11y.grabbed_keys_len; i++) {
		if (a11y.grabbed_keys[i].keycode == keycode) {
			return &a11y.grabbed_keys[i];
		}
	}
	return NULL;
}

static void add_grabbed_key(uint32_t keycode, uint32_t keysym,
		bool modifier_tap) {
	struct a11y_grabbed_key *key = find_grabbed_key(keycode);
	if (!key) {
		if (a11y.grabbed_keys_len == WLR_KEYBOARD_KEYS_CAP) {
			return;
		}
		key = &a11y.grabbed_keys[a11y.grabbed_keys_len++];
	}
	*key = (struct a11y_grabbed_key){
		.keycode = keycode,
		.keysym = keysym,
		.modifier_tap = modifier_tap,
	};
}

static void remove_grabbed_key(struct a11y_grabbed_key *key) {
	*key = a11y.grabbed_keys[--a11y.grabbed_keys_len];
}

/**
 * The modifier mask in effect, including locked modifiers (Caps Lock, Num
 * Lock), in X11 bit order: Shift 1, Lock 2, Control 4, Mod1 8 ... Mod5 128.
 * wlr_keyboard_get_modifiers() leaves the locked ones out.
 */
static uint32_t get_x11_state(struct wlr_keyboard *wlr_keyboard) {
	static const char *const names[] = {
		"Shift", "Lock", "Control", "Mod1", "Mod2", "Mod3", "Mod4", "Mod5",
	};
	if (!wlr_keyboard->keymap || !wlr_keyboard->xkb_state) {
		return 0;
	}
	uint32_t state = 0;
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		xkb_mod_index_t index =
			xkb_keymap_mod_get_index(wlr_keyboard->keymap, names[i]);
		if (index != XKB_MOD_INVALID && xkb_state_mod_index_is_active(
				wlr_keyboard->xkb_state, index, XKB_STATE_MODS_EFFECTIVE) > 0) {
			state |= 1u << i;
		}
	}
	return state;
}

static bool process_key(struct wlr_keyboard *wlr_keyboard,
		struct wlr_keyboard_key_event *event) {
	// The keyboard's xkb state does not include this event yet
	uint32_t keycode = event->keycode;
	xkb_keycode_t xkb_keycode = keycode + 8;
	if (!wlr_keyboard->xkb_state) {
		return false;
	}
	xkb_keysym_t keysym =
		xkb_state_key_get_one_sym(wlr_keyboard->xkb_state, xkb_keycode);
	uint32_t unichar = xkb_keysym_to_utf32(keysym);
	uint32_t state = get_x11_state(wlr_keyboard);
	bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
	uint32_t time_msec = event->time_msec;
	uint32_t repeat_delay = wlr_keyboard->repeat_info.delay > 0 ?
		(uint32_t)wlr_keyboard->repeat_info.delay : 0;
	bool soon = a11y.have_last &&
		time_msec - a11y.last_time_msec < repeat_delay;

	struct a11y_client *client;
	bool grabbed = false;
	if (pressed) {
		bool modifier = is_grabbed_modifier(keysym);
		// A grabbed modifier pressed again quickly, with no other key in
		// between, is handled normally
		bool second_tap = modifier && a11y.tap_armed &&
			a11y.tap_keycode == keycode && soon;
		a11y.tap_armed = false;

		wl_list_for_each(client, &a11y.clients, link) {
			if (client_watches_key(client, keycode, keysym, state)) {
				emit_key_event(client, false, state, keysym, unichar,
					xkb_keycode);
				client_add_reported_key(client, keycode);
			}
		}

		if (!second_tap) {
			wl_list_for_each(client, &a11y.clients, link) {
				if (client_grabs_key(client, keycode, keysym, state)) {
					grabbed = true;
					break;
				}
			}
		}
		if (grabbed) {
			add_grabbed_key(keycode, keysym, modifier);
			if (modifier) {
				// The modifier's own tap leaves the keyboard state alone
				event->update_state = false;
			}
		}
	} else {
		// A release goes where its press went
		struct a11y_grabbed_key *key = find_grabbed_key(keycode);
		grabbed = key != NULL;

		// A client that was told about the press is told about the release,
		// so that it never sees a key stuck down
		wl_list_for_each(client, &a11y.clients, link) {
			bool had_press = client_remove_reported_key(client, keycode);
			if (had_press ||
					client_watches_key(client, keycode, keysym, state)) {
				emit_key_event(client, true, state, keysym, unichar,
					xkb_keycode);
			}
		}

		a11y.tap_armed = key && key->modifier_tap && a11y.last_pressed &&
			a11y.last_keycode == keycode && soon;
		a11y.tap_keycode = keycode;
		if (key) {
			if (key->modifier_tap) {
				event->update_state = false;
			}
			remove_grabbed_key(key);
		}
	}

	a11y.have_last = true;
	a11y.last_keycode = keycode;
	a11y.last_pressed = pressed;
	a11y.last_time_msec = time_msec;
	return grabbed;
}

bool a11y_handle_key(struct sway_keyboard *keyboard,
		struct wlr_keyboard_key_event *event) {
	if (!a11y.initialized) {
		return false;
	}

	if (a11y.pending.valid && a11y.pending.event == event &&
			a11y.pending.keycode == event->keycode &&
			a11y.pending.state == event->state &&
			a11y.pending.time_msec == event->time_msec) {
		// Second pass of a grouped keyboard's event
		a11y.pending.valid = false;
		return a11y.pending.grabbed;
	}
	a11y.pending.valid = false;

	if (wl_list_empty(&a11y.clients) && a11y.grabbed_keys_len == 0) {
		a11y.tap_armed = false;
		return false;
	}

	bool grabbed = process_key(keyboard->wlr, event);

	if (keyboard->wlr->group ||
			wlr_keyboard_group_from_wlr_keyboard(keyboard->wlr)) {
		a11y.pending.valid = true;
		a11y.pending.event = event;
		a11y.pending.keycode = event->keycode;
		a11y.pending.time_msec = event->time_msec;
		a11y.pending.state = event->state;
		a11y.pending.grabbed = grabbed;
	}

	// Write out queued signals, or wait for the socket to become writable
	update_bus_sources();
	return grabbed;
}
