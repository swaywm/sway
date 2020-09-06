#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/security.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"

struct find_by_cid_data {
	uint32_t cid;
	struct wl_client *target;
};

static void find_by_cid_iter(struct sway_container *container, void* data) {
	struct find_by_cid_data *ctx = data;
	if (!container->view)
		return;
	if (ctx->cid != container->node.id)
		return;
	if (!container->view->surface)
		return;
	ctx->target = wl_resource_get_client(container->view->surface->resource);
}

static void do_cmd(struct wl_client *target, int mode, uint32_t privs) {
	uint32_t allow;
	switch (mode) {
	case 0:
		security_set_permit(target, privs);
		break;
	case 1:
		allow = security_get_permit(target);
		allow |= privs;
		security_set_permit(target, allow);
		break;
	case 2:
		allow = security_get_permit(target);
		allow &= ~privs;
		security_set_permit(target, allow);
		break;
	}
}

struct cmd_results *cmd_security(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "security", EXPECTED_AT_LEAST, 3))) {
		return error;
	}

	char* op = argv[0];
	char* who = argv[1];
	uint32_t privs = 0;
	int mode;
	if (strcmp(op, "set") == 0) {
		mode = 0;
	} else if (strcmp(op, "permit") == 0) {
		mode = 1;
	} else if (strcmp(op, "deny") == 0) {
		mode = 2;
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid operation", op);
	}

	for(int i = 2; i < argc; i++) {
		char* arg = argv[i];
		if (strcmp(arg, "*") == 0)
			privs |= ~0;
		else if (strcmp(arg, "layer_shell") == 0)
			privs |= 1 << PRIV_LAYER_SHELL;
		else if (strcmp(arg, "output_manager") == 0)
			privs |= 1 << PRIV_OUTPUT_MANAGER;
		else if (strcmp(arg, "input_method") == 0)
			privs |= 1 << PRIV_INPUT_METHOD;
		else if (strcmp(arg, "text_input") == 0)
			privs |= 1 << PRIV_TEXT_INPUT;
		else if (strcmp(arg, "foreign_toplevel_manager") == 0)
			privs |= 1 << PRIV_FOREIGN_TOPLEVEL_MANAGER;
		else if (strcmp(arg, "dmabuf_manager") == 0)
			privs |= 1 << PRIV_DMABUF_MANAGER;
		else if (strcmp(arg, "screencopy_manager") == 0)
			privs |= 1 << PRIV_SCREENCOPY_MANAGER;
		else if (strcmp(arg, "data_control_manager") == 0)
			privs |= 1 << PRIV_DATA_CONTROL_MANAGER;
		else if (strcmp(arg, "gamma_control_manager") == 0)
			privs |= 1 << PRIV_GAMMA_CONTROL_MANAGER;
		else if (strcmp(arg, "input_inhibit") == 0)
			privs |= 1 << PRIV_INPUT_INHIBIT;
		else if (strcmp(arg, "input_keyboard_shortcuts_inhibit") == 0)
			privs |= 1 << PRIV_INPUT_KEYBOARD_SHORTCUTS_INHIBIT;
		else if (strcmp(arg, "input_virtual_keyboard") == 0)
			privs |= 1 << PRIV_INPUT_VIRTUAL_KEYBOARD;
		else if (strcmp(arg, "input_virtual_pointer") == 0)
			privs |= 1 << PRIV_INPUT_VIRTUAL_POINTER;
		else if (strcmp(arg, "output_power_manager") == 0)
			privs |= 1 << PRIV_OUTPUT_POWER_MANAGER;
		else
			return cmd_results_new(CMD_INVALID, "Unknown permission", arg);
	}

	if (strcmp(who, "*") == 0) {
		do_cmd(NULL, mode, privs);
	} else if (strncmp(who, "cid=", 4) == 0) {
		struct find_by_cid_data data = {
			.cid = strtoul(who + 4, NULL, 0),
		};
		root_for_each_container(find_by_cid_iter, &data);
		if (!data.target)
			return cmd_results_new(CMD_INVALID, "Client not found", who);
		do_cmd(data.target, mode, privs);
	} else if (strncmp(who, "pid=", 4) == 0) {
		pid_t target_pid = strtoul(who + 4, NULL, 0);
		struct wl_client* client;
		struct wl_list *clients = wl_display_get_client_list(server.wl_display);
		wl_client_for_each(client, clients) {
			pid_t pid;
			wl_client_get_credentials(client, &pid, NULL, NULL);
			if (pid == target_pid)
				do_cmd(client, mode, privs);
		}
	} else {
		// TODO support other criteria?  Only secure things like pid
		return cmd_results_new(CMD_INVALID, "Invalid target exmpression", who);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
