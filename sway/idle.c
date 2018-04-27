#define _POSIX_C_SOURCE 200112L
#include <sys/types.h>
#include <unistd.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <sway/server.h>
#include <sway/output.h>
#include <sway/layers.h>
#include <sway/config.h>
#include <wlr/config.h>
#include <wlr/backend/session.h>
#include <wlr/backend/multi.h>


void invoke_swaylock() {
	int pid = fork();
	if (pid == 0) {
		char *const cmd[] = { "sh", "-c", config->swaylock_command, NULL, };
		execvp(cmd[0], cmd);
		exit(1);
	}
	wlr_log(L_DEBUG, "Spawned swaylock %d", pid);
}

bool have_lock() {
	if (root_container.children == NULL)
		return false;
	if (root_container.children->items == NULL)
		return false;
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output_container =
			root_container.children->items[i];
		if (output_container == NULL)
			return false;
		struct sway_output *output =
			output_container->sway_output;
		if (output == NULL)
			return false;
		if (output->layers == NULL)
			return false;
		struct wl_list *layers = &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY];
		if (layers == NULL)
			return false;
		struct sway_layer_surface *sway_layer;
		wl_list_for_each_reverse(sway_layer, layers, link) {
			struct wlr_layer_surface *surface = sway_layer->layer_surface;
			if (surface != NULL && surface->namespace != NULL) {
				if (!strcmp("lockscreen", surface->namespace))
					wlr_log(L_DEBUG, "Lockscreen found!");
				return true;
			}
		}
	}

	wlr_log(L_DEBUG, "No lock");
	return false;
}

static int fd = -1;
static int inhibit_cnt=0;

void cleanup_inhibit() {
	if (fd >= 0) 
		close(fd);  //Release lock
	fd = -1;
	inhibit_cnt=0;
	wlr_log(L_DEBUG, "Cleanup inhibit");
	return;
}

static int check_for_lock(void *data) {
	struct sway_server *server = data;
	if(have_lock()) { //If we for some reason already have a lockscreen
		wlr_log(L_INFO, "Got lock, will release inhibit lock");
		cleanup_inhibit();
		return 0;
	}

	if(inhibit_cnt > 4) {
		wlr_log(L_INFO, "Reached inhibit timeout, releasing lock");
		cleanup_inhibit();
		return 0;
	}

	inhibit_cnt++;
	struct wl_event_source *source = wl_event_loop_add_timer(server->wl_event_loop, check_for_lock, server);
    wl_event_source_timer_update(source, 100);
	return 0;
}

static void prepare_for_sleep(struct wlr_session *session, void *data) {
	struct sway_server *server = data;
	wlr_log(L_INFO, "PrepareForSleep signal received");
	if(have_lock()) { //If we for some reason already have a lockscreen
		wlr_log(L_INFO, "Already have lock, no inhibit");
		cleanup_inhibit();
		return;
	}


	wlr_log(L_DEBUG, "Trying to inhibit sleep");
	fd = wlr_session_inhibit_sleep(session);
	invoke_swaylock();
	if (fd>=0) {
		struct wl_event_source *source = wl_event_loop_add_timer(server->wl_event_loop, check_for_lock, server);
		wl_event_source_timer_update(source, 100);
	}
	return;
}

void setup_sleep_listener(struct sway_server *server) {


	struct wlr_session *session = NULL;
	if (wlr_backend_is_multi(server->backend)) {
		session = wlr_multi_get_session(server->backend);
	}
	if (!session) {
		wlr_log(L_INFO, "No supported session found, skipping sleep litener setup");
	}

	wlr_session_prepare_for_sleep_listen(session, prepare_for_sleep, server);
}

static int handle_idle(void* data) {
	wlr_log(L_DEBUG, "Idle state");
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
	wlr_log(L_DEBUG, "Active state");
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

void idle_setup_seat(struct sway_server *server, struct sway_seat *seat) {
	if (server->idle == NULL) {
		return;
	}
	if (config != NULL) {
		if (config->idle_timeout > 0) {
			wlr_log(L_DEBUG, "Setup idle timer %d", config->idle_timeout);
			wlr_idle_listen(server->idle, config->idle_timeout * 1000, &idle_listener, seat->wlr_seat);
		} else {
			wlr_log(L_INFO, "Idle timeout set to 0, will disable screen power management");
		}
		if (config->lock_timeout > 0) {
			wlr_log(L_DEBUG, "Setup lock timer %d", config->lock_timeout);
			wlr_idle_listen(server->idle, config->lock_timeout * 1000, &lock_listener, seat->wlr_seat);
			setup_sleep_listener(server);
		} else {
			wlr_log(L_INFO, "Lock timeout set to 0, will disable auto lock");
		}
	} else {
		wlr_log(L_ERROR, "Cant setup idle timers for seat since no config is available!");
	}
}

bool idle_init(struct sway_server *server) {
	wlr_log(L_DEBUG, "Initializing idle");
	server->idle = wlr_idle_create(server->wl_display);
	struct sway_seat *seat = NULL;
	wl_list_for_each(seat, &input_manager->seats, link) {
		idle_setup_seat(server, seat);
	}
	return true;
}

