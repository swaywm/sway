#include <wlr/types/wlr_action_binder_v1.h>

void action_binder_v1_bind(struct wl_listener *listener, void *data) {
	struct wlr_action_binder_v1_state *state = data;
	struct wlr_action_binding_v1 *binding = NULL, *tmp = NULL;
	wl_list_for_each_safe(binding, tmp, &state->bind_queue, link) {
		wlr_action_binder_v1_bind(binding, "");
	}
}

void action_binder_v1_unbind(struct wl_listener *listener, void *data) {
}

void action_binder_v1_delete(struct wl_listener *listener, void *data) {
}
