#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <wayland-server-core.h>
#include "sway/security.h"

struct sway_security_rule {
	char *command;
	char *global;
	struct wl_list link;
};

static struct wl_list rules;

static struct sway_security_rule *security_rule_create(const char *cmd,
		const char *global) {
	struct sway_security_rule *rule =
		calloc(1, sizeof(struct sway_security_rule));
	if (rule == NULL) {
		wlr_log(WLR_ERROR, "Allocation failed");
		return NULL;
	}
	if (cmd != NULL) {
		rule->command = strdup(cmd);
		if (rule->command == NULL) {
			goto err;
		}
	}
	if (global != NULL) {
		rule->global = strdup(global);
		if (rule->global == NULL) {
			goto err;
		}
	}
	wl_list_insert(&rules, &rule->link);
	return rule;

err:
	wlr_log(WLR_ERROR, "Allocation failed");
	free(rule->global);
	free(rule->command);
	free(rule);
	return NULL;
}

static bool command_from_pid(char cmd[static PATH_MAX + 1], pid_t pid) {
#ifdef __linux__
	char link_path[PATH_MAX];
	snprintf(link_path, sizeof(link_path), "/proc/%d/exe", pid);

	ssize_t n = readlink(link_path, cmd, PATH_MAX);
	if (n < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to readlink() %s", link_path);
		return false;
	}
	cmd[n] = '\0';
	return true;
#else
	return false;
#endif
}

static bool global_filter(const struct wl_client *client,
		const struct wl_global *global, void *data) {
	pid_t pid = 0;
	wl_client_get_credentials((struct wl_client *)client, &pid, NULL, NULL);
	if (pid == 0) {
		wlr_log(WLR_DEBUG, "Host doesn't support Wayland credentials, "
			"cannot enforce security rules");
		return true;
	}

	char cmd[PATH_MAX + 1];
	if (!command_from_pid(cmd, pid)) {
		wlr_log(WLR_ERROR, "Failed to get command path from PID %d", pid);
		return false;
	}

	const struct wl_interface *interface = wl_global_get_interface(global);
	bool ok = check_security_rule(cmd, interface->name);
	if (ok) {
		wlr_log(WLR_DEBUG, "Allowing %s to bind to %s", cmd, interface->name);
	} else {
		wlr_log(WLR_DEBUG, "Denying %s from binding to %s", cmd, interface->name);
	}
	return ok;
}

bool load_security(struct wl_display *display) {
	wl_list_init(&rules);

	wl_display_set_global_filter(display, global_filter, NULL);

	// TODO: move this in a file
	security_rule_create(NULL, "wl_shm");
	security_rule_create(NULL, "wl_drm");
	security_rule_create(NULL, "wl_compositor");
	security_rule_create(NULL, "wl_subcompositor");
	security_rule_create(NULL, "wl_data_device_manager");
	security_rule_create(NULL, "wl_seat");
	security_rule_create(NULL, "wl_output");
	security_rule_create(NULL, "zwp_linux_dmabuf_v1");
	security_rule_create(NULL, "gtk_primary_selection_device_manager");
	security_rule_create(NULL, "zxdg_output_manager_v1");
	security_rule_create(NULL, "org_kde_kwin_idle");
	security_rule_create(NULL, "zwp_idle_inhibit_manager_v1");
	security_rule_create(NULL, "zxdg_shell_v6");
	security_rule_create(NULL, "xdg_wm_base");
	security_rule_create(NULL, "org_kde_kwin_server_decoration_manager");
	security_rule_create(NULL, "zxdg_decoration_manager_v1");
	security_rule_create(NULL, "wp_presentation");
	// gamma_control_manager
	// zwlr_gamma_control_manager_v1
	// zwlr_layer_shell_v1
	// zwlr_export_dmabuf_manager_v1
	// zwlr_screencopy_manager_v1
	// zwp_virtual_keyboard_manager_v1
	// zwlr_input_inhibit_manager_v1

	return true;
}

void finish_security(void) {
	struct sway_security_rule *rule, *tmp;
	wl_list_for_each_safe(rule, tmp, &rules, link) {
		free(rule->command);
		free(rule->global);
		wl_list_remove(&rule->link);
		free(rule);
	}

	wl_list_remove(&rules);
}

bool check_security_rule(const char *cmd, const char *global) {
	struct sway_security_rule *rule;
	wl_list_for_each(rule, &rules, link) {
		if (rule->command != NULL && strcmp(cmd, rule->command) != 0) {
			continue;
		}
		if (rule->global != NULL && strcmp(global, rule->global) != 0) {
			continue;
		}
		return true;
	}
	return false;
}


struct command_policy *alloc_command_policy(const char *command) {
	struct command_policy *policy = calloc(1, sizeof(struct command_policy));
	if (!policy) {
		return NULL;
	}
	policy->command = strdup(command);
	if (!policy->command) {
		free(policy);
		return NULL;
	}
	policy->context = 0;
	return policy;
}
