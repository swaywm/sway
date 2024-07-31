#include <wlr/types/wlr_action_binder_v1.h>
#include "stringop.h"

void action_binder_v1_bind(struct wl_listener *listener, void *data) {
	struct wlr_action_binder_v1_state *state = data;
	struct wlr_action_binding_v1 *binding = NULL, *tmp = NULL;
	wl_list_for_each_safe(binding, tmp, &state->bind_queue, link) {
		char *msg = format_str("User defined trigger for %s:%s in the config file.",
				binding->namespace, binding->name);
		wlr_action_binding_v1_bind(binding, msg ? msg : "");
	}
}

void action_binder_v1_delete(struct wl_listener *listener, void *data) {
}
