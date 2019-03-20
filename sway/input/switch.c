#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/input/switch.h"
#include <wlr/types/wlr_idle.h>
#include "log.h"

struct sway_switch *sway_switch_create(struct sway_seat *seat,
        struct sway_seat_device *device) {
    struct sway_switch *switch_device =
        calloc(1, sizeof(struct sway_switch));
    if (!sway_assert(switch_device, "could not allocate switch")) {
        return NULL;
    }
    device->switch_device = switch_device;
    switch_device->seat_device = device;
    wl_list_init(&switch_device->switch_toggle.link);
    sway_log(SWAY_DEBUG, "Allocated switch for device");

    return switch_device;
}

static void handle_switch_toggle(struct wl_listener *listener, void *data) {
    struct sway_switch *sway_switch =
            wl_container_of(listener, sway_switch, switch_toggle);
    struct sway_seat* seat = sway_switch->seat_device->sway_seat;
    struct wlr_seat *wlr_seat = seat->wlr_seat;
    struct wlr_input_device *wlr_device =
        sway_switch->seat_device->input_device->wlr_device;

    wlr_idle_notify_activity(server.idle, wlr_seat);
    bool input_inhibited = seat->exclusive_client != NULL;

    char *device_identifier = input_device_get_identifier(wlr_device);

    struct wlr_event_switch_toggle *event = data;
    enum wlr_switch_type type = event->switch_type;
    enum wlr_switch_state state = event->switch_state;
    sway_log(SWAY_DEBUG, "%s: type %d state %d", device_identifier, type, state);

    list_t *bindings = config->current_mode->switch_bindings;
    for (int i = 0; i < bindings->length; ++i) {
        struct sway_switch_binding *binding = bindings->items[i];
        if (binding->type != type) {
            continue;
        }
        if (binding->state != WLR_SWITCH_STATE_TOGGLE &&
                binding->state != state) {
            continue;
        }
        bool binding_locked = binding->flags & BINDING_LOCKED;
        if (!binding_locked && input_inhibited) {
            continue;
        }

        struct sway_binding *dummy_binding = calloc(1, sizeof(struct sway_binding));
        dummy_binding->type = BINDING_SWITCH;
        dummy_binding->flags = binding->flags;
        dummy_binding->command = binding->command;

        seat_execute_command(seat, dummy_binding);
        free(dummy_binding);
    }

    transaction_commit_dirty();

    free(device_identifier);
}

void sway_switch_configure(struct sway_switch *sway_switch) {
    struct wlr_input_device *wlr_device =
        sway_switch->seat_device->input_device->wlr_device;
    wl_list_remove(&sway_switch->switch_toggle.link);
    wl_signal_add(&wlr_device->switch_device->events.toggle,
            &sway_switch->switch_toggle);
    sway_switch->switch_toggle.notify = handle_switch_toggle;
    sway_log(SWAY_DEBUG, "Configured switch for device");
}

void sway_switch_destroy(struct sway_switch *sway_switch) {
    if (!sway_switch) {
        return;
    }
    wl_list_remove(&sway_switch->switch_toggle.link);
    free(sway_switch);
}
