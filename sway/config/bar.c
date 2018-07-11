#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <strings.h>
#include "sway/config.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

static void terminate_swaybar(pid_t pid) {
	wlr_log(WLR_DEBUG, "Terminating swaybar %d", pid);
	int ret = kill(-pid, SIGTERM);
	if (ret != 0) {
		wlr_log_errno(WLR_ERROR, "Unable to terminate swaybar %d", pid);
	} else {
		int status;
		waitpid(pid, &status, 0);
	}
}

void free_bar_config(struct bar_config *bar) {
	if (!bar) {
		return;
	}
	free(bar->id);
	free(bar->mode);
	free(bar->position);
	free(bar->hidden_state);
	free(bar->status_command);
	free(bar->font);
	free(bar->separator_symbol);
	// TODO: Free mouse bindings
	list_free(bar->bindings);
	if (bar->outputs) {
		free_flat_list(bar->outputs);
	}
	if (bar->pid != 0) {
		terminate_swaybar(bar->pid);
	}
	free(bar->colors.background);
	free(bar->colors.statusline);
	free(bar->colors.separator);
	free(bar->colors.focused_background);
	free(bar->colors.focused_statusline);
	free(bar->colors.focused_separator);
	free(bar->colors.focused_workspace_border);
	free(bar->colors.focused_workspace_bg);
	free(bar->colors.focused_workspace_text);
	free(bar->colors.active_workspace_border);
	free(bar->colors.active_workspace_bg);
	free(bar->colors.active_workspace_text);
	free(bar->colors.inactive_workspace_border);
	free(bar->colors.inactive_workspace_bg);
	free(bar->colors.inactive_workspace_text);
	free(bar->colors.urgent_workspace_border);
	free(bar->colors.urgent_workspace_bg);
	free(bar->colors.urgent_workspace_text);
	free(bar->colors.binding_mode_border);
	free(bar->colors.binding_mode_bg);
	free(bar->colors.binding_mode_text);
	free(bar);
}

struct bar_config *default_bar_config(void) {
	struct bar_config *bar = NULL;
	bar = calloc(1, sizeof(struct bar_config));
	if (!bar) {
		return NULL;
	}
	bar->outputs = NULL;
	bar->position = strdup("bottom");
	bar->pango_markup = false;
	bar->swaybar_command = NULL;
	bar->font = NULL;
	bar->height = -1;
	bar->workspace_buttons = true;
	bar->wrap_scroll = false;
	bar->separator_symbol = NULL;
	bar->strip_workspace_numbers = false;
	bar->binding_mode_indicator = true;
	bar->verbose = false;
	bar->pid = 0;
	if (!(bar->mode = strdup("dock"))) {
	       goto cleanup;
	}
	if (!(bar->hidden_state = strdup("hide"))) {
		goto cleanup;
	}
	if (!(bar->bindings = create_list())) {
		goto cleanup;
	}
	if (!(bar->status_command =
			strdup("while date +'%Y-%m-%d %l:%M:%S %p'; do sleep 1; done"))) {
		goto cleanup;
	}
	// set default colors
	if (!(bar->colors.background = strndup("#000000ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.statusline = strndup("#ffffffff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.separator = strndup("#666666ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.focused_workspace_border = strndup("#4c7899ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.focused_workspace_bg = strndup("#285577ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.focused_workspace_text = strndup("#ffffffff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.active_workspace_border = strndup("#333333ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.active_workspace_bg = strndup("#5f676aff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.active_workspace_text = strndup("#ffffffff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.inactive_workspace_border = strndup("#333333ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.inactive_workspace_bg = strndup("#222222ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.inactive_workspace_text = strndup("#888888ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.urgent_workspace_border = strndup("#2f343aff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.urgent_workspace_bg = strndup("#900000ff", 9))) {
		goto cleanup;
	}
	if (!(bar->colors.urgent_workspace_text = strndup("#ffffffff", 9))) {
		goto cleanup;
	}
	// if the following colors stay undefined, they fall back to background,
	// statusline, separator and urgent_workspace_*.
	bar->colors.focused_background = NULL;
	bar->colors.focused_statusline = NULL;
	bar->colors.focused_separator = NULL;
	bar->colors.binding_mode_border = NULL;
	bar->colors.binding_mode_bg = NULL;
	bar->colors.binding_mode_text = NULL;

	list_add(config->bars, bar);
	return bar;
cleanup:
	free_bar_config(bar);
	return NULL;
}

void invoke_swaybar(struct bar_config *bar) {
	// Pipe to communicate errors
	int filedes[2];
	if (pipe(filedes) == -1) {
		wlr_log(WLR_ERROR, "Pipe setup failed! Cannot fork into bar");
		return;
	}

	bar->pid = fork();
	if (bar->pid == 0) {
		setpgid(0, 0);
		close(filedes[0]);

		// run custom swaybar
		size_t len = snprintf(NULL, 0, "%s -b %s",
				bar->swaybar_command ? bar->swaybar_command : "swaybar",
				bar->id);
		char *command = malloc(len + 1);
		if (!command) {
			const char msg[] = "Unable to allocate swaybar command string";
			size_t msg_len = sizeof(msg);
			if (write(filedes[1], &msg_len, sizeof(size_t))) {};
			if (write(filedes[1], msg, msg_len)) {};
			close(filedes[1]);
			exit(1);
		}
		snprintf(command, len + 1, "%s -b %s",
				bar->swaybar_command ? bar->swaybar_command : "swaybar",
				bar->id);
		char *const cmd[] = { "sh", "-c", command, NULL, };
		close(filedes[1]);
		execvp(cmd[0], cmd);
		exit(1);
	}
	wlr_log(WLR_DEBUG, "Spawned swaybar %d", bar->pid);
	close(filedes[0]);
	size_t len;
	if (read(filedes[1], &len, sizeof(size_t)) == sizeof(size_t)) {
		char *buf = malloc(len);
		if(!buf) {
			wlr_log(WLR_ERROR, "Cannot allocate error string");
			return;
		}
		if (read(filedes[1], buf, len)) {
			wlr_log(WLR_ERROR, "%s", buf);
		}
		free(buf);
	}
	close(filedes[1]);
}

static bool active_output(const char *name) {
	struct sway_container *cont = NULL;
	for (int i = 0; i < root_container.children->length; ++i) {
		cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT && strcasecmp(name, cont->name) == 0) {
			return true;
		}
	}
	return false;
}

void load_swaybars(void) {
	for (int i = 0; i < config->bars->length; ++i) {
		struct bar_config *bar = config->bars->items[i];
		bool apply = false;
		if (bar->outputs) {
			for (int j = 0; j < bar->outputs->length; ++j) {
				char *o = bar->outputs->items[j];
				if (!strcmp(o, "*") || active_output(o)) {
					apply = true;
					break;
				}
			}
		} else {
			apply = true;
		}
		if (apply) {
			if (bar->pid != 0) {
				terminate_swaybar(bar->pid);
			}
			wlr_log(WLR_DEBUG, "Invoking swaybar for bar id '%s'", bar->id);
			invoke_swaybar(bar);
		}
	}
}
