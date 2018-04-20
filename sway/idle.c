#define _POSIX_C_SOURCE 200112L
#include <sys/types.h>
#include <unistd.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <sway/server.h>
#include <sway/output.h>
#include <sway/config.h>

void invoke_swaylock() {
	int pid = fork();
	if (pid == 0) {
		char *const cmd[] = { "sh", "-c", config->swaylock_command, NULL, };
		execvp(cmd[0], cmd);
		exit(1);
	}
	wlr_log(L_DEBUG, "Spawned swaylock %d", pid);
}

static int handle_idle(void* data) {
	wlr_log(L_ERROR, "Idle state");
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type != C_OUTPUT) {
			continue;
		}
		if (cont->sway_output && cont->sway_output->wlr_output) {
			wlr_output_enable(cont->sway_output->wlr_output, false);
		}
	}
	return 0;
}

static int handle_resume(void *data) {
	wlr_log(L_ERROR, "Active state");
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type != C_OUTPUT) {
			continue;
		}
		if (cont->sway_output && cont->sway_output->wlr_output) {
			wlr_output_enable(cont->sway_output->wlr_output, true);
		}
	}
	return 0;
}

static const struct wlr_idle_timeout_listener idle_listener = {
	.idle = handle_idle,
	.resumed = handle_resume,
};

static int handle_lock(void* data) {
	wlr_log(L_DEBUG, "Lock screen");
	invoke_swaylock();
	return 0;
}

static int handle_nop(void *data) {
	//NOP
	return 0;
}


static const struct wlr_idle_timeout_listener lock_listener = {
	.idle = handle_lock,
	.resumed = handle_nop,
};

bool idle_init(struct sway_server *server) {
	wlr_log(L_DEBUG, "Initializing idle");
	server->idle = wlr_idle_create(server->wl_display);
	wlr_log(L_DEBUG, "Setup idle timer %d", config->idle_timeout);
	wlr_idle_listen(server->idle, config->idle_timeout * 1000, &idle_listener); 
	wlr_log(L_DEBUG, "Setup lock timer %d", config->lock_timeout);
	wlr_idle_listen(server->idle, config->lock_timeout * 1000, &lock_listener); 
	return true;
}
