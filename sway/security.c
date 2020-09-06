#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/noop.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_gtk_primary_selection.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include "config.h"
#include "list.h"
#include "log.h"
#include "sway/config.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/input/input-manager.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/security.h"
#include "sway/tree/root.h"

struct security_permit {
	uint32_t allowed;
	const struct wl_client *client;
	struct wl_listener on_destroy;
};

static uint32_t security_allow_default = ~0;
static list_t *security_permit_list;

bool security_global_filter(const struct wl_client *client, const struct wl_global *global, void *data)
{
	struct sway_server *server = data;
	enum security_perm perm;

	if (global == server->layer_shell->global)
		perm = PRIV_LAYER_SHELL;
	else if (global == server->output_manager_v1->global)
		perm = PRIV_OUTPUT_MANAGER;
	else if (global == server->input_method->global)
		perm = PRIV_INPUT_METHOD;
	else if (global == server->text_input->global)
		perm = PRIV_TEXT_INPUT;
	else if (global == server->foreign_toplevel_manager->global)
		perm = PRIV_FOREIGN_TOPLEVEL_MANAGER;
	else if (global == server->dmabuf_manager->global)
		perm = PRIV_DMABUF_MANAGER;
	else if (global == server->screencopy_manager->global )
		perm = PRIV_SCREENCOPY_MANAGER;
	else if (global == server->data_control_manager->global)
		perm = PRIV_DATA_CONTROL_MANAGER;
	else if (global == server->gamma_control_manager->global)
		perm = PRIV_GAMMA_CONTROL_MANAGER;
	else if (global == server->input->inhibit->global)
		perm = PRIV_INPUT_INHIBIT;
	else if (global == server->input->keyboard_shortcuts_inhibit->global)
		perm = PRIV_INPUT_KEYBOARD_SHORTCUTS_INHIBIT;
	else if (global == server->input->virtual_keyboard->global)
		perm = PRIV_INPUT_VIRTUAL_KEYBOARD;
	else if (global == server->input->virtual_pointer->global)
		perm = PRIV_INPUT_VIRTUAL_POINTER;
	else if (global == server->output_power_manager_v1->global)
		perm = PRIV_OUTPUT_POWER_MANAGER;
	else
		return true;

	uint32_t priv_bit = 1 << perm;
	uint32_t allowed = security_get_permit(client);
	if (allowed & priv_bit)
		return true;

	return false;
}

static void security_client_destroy(struct wl_listener *listener, void *client_v)
{
	const struct wl_client *client = client_v;
	if (!security_permit_list)
		return;

	for (int i = 0; i < security_permit_list->length; ++i) {
		struct security_permit *item = security_permit_list->items[i];
		if (client != item->client)
			continue;
		list_del(security_permit_list, i);
		free(item);
		return;
	}
}


uint32_t security_get_permit(const struct wl_client *client)
{
	if (client && security_permit_list) {
		for (int i = 0; i < security_permit_list->length; ++i) {
			struct security_permit *item = security_permit_list->items[i];
			if (client == item->client)
				return item->allowed;
		}
	}
	return security_allow_default;
}

void security_set_permit(struct wl_client *client, uint32_t allowed) {
	struct security_permit *found = NULL;
	if (client == NULL) {
		security_allow_default = allowed;
		return;
	}
	if (!security_permit_list)
		security_permit_list = create_list();
	for (int i = 0; i < security_permit_list->length; ++i) {
		struct security_permit *item = security_permit_list->items[i];
		if (client == item->client) {
			found = item;
			break;
		}
	}
	if (!found) {
		found = malloc(sizeof(*found));
		found->client = client;
		found->on_destroy.notify = security_client_destroy;
		list_add(security_permit_list, found);
		wl_client_add_destroy_listener(client, &found->on_destroy);
	}
	found->allowed = allowed;
}
