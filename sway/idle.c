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
#ifdef WLR_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#elif defined(WLR_HAS_ELOGIND)
#include <elogind/sd-bus.h>
#include <elogind/sd-login.h>
#endif

void invoke_swaylock() {
	int pid = fork();
	if (pid == 0) {
		char *const cmd[] = { "sh", "-c", config->swaylock_command, NULL, };
		execvp(cmd[0], cmd);
		exit(1);
	}
	wlr_log(L_DEBUG, "Spawned swaylock %d", pid);
}

#if defined(WLR_HAS_SYSTEMD) || defined(WLR_HAS_ELOGIND)

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
			if (!strcmp("lockscreen", surface->namespace))
				wlr_log(L_DEBUG, "Lockscreen found!");
			return true;
		}
	}

	wlr_log(L_DEBUG, "No lock");
	return false;
}

static int fd = 0;
static int inhibit_cnt=0;

static int cleanup_inhibit(void *data) {
	fd = 0;
	inhibit_cnt=0;
	wlr_log(L_DEBUG, "Cleanup inhibit");
	return 0;
}

static int prepare_for_sleep(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error) {
	struct sway_server *server = userdata;

	wlr_log(L_INFO, "PrepareForSleep signal received");
	if(inhibit_cnt > 4 || have_lock()) {
		wlr_log(L_INFO, "Already have lock, no inhibit");
		cleanup_inhibit(NULL);
		return 0;
	}

	wlr_log(L_INFO, "No lock, will inhibit");

	struct sd_bus *bus;
	int ret = sd_bus_default_system(&bus);
	if (ret < 0) {
		wlr_log(L_ERROR, "Failed to open D-Bus connection: %s", strerror(-ret));
		return 0;
	}

	ret = sd_bus_call_method(bus, "org.freedesktop.login1",
			"/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Inhibit",
			ret_error, &msg, "ssss", "sleep", "sway-idle", "Setup Up Lock Screen", "delay");
	if (ret < 0) {
		wlr_log(L_ERROR, "Failed to send Inhibit signal: %s",
				strerror(-ret));
	} else {
		ret = sd_bus_message_read(msg, "h", &fd);
		if (ret < 0) {
			wlr_log(L_ERROR, "Failed to parse D-Bus response for Inhibit: %s", strerror(-ret));
		}
	}

	if (!inhibit_cnt) {
		invoke_swaylock();

		// 3 seconds should be well enough for 5 inhibits to be over and done
		struct wl_event_source *source = wl_event_loop_add_timer(server->wl_event_loop, cleanup_inhibit, NULL);
		wl_event_source_timer_update(source, 3000);
	}
	inhibit_cnt++;

	wlr_log(L_ERROR, "Inhibit done %d", inhibit_cnt);
	return 0;
}

void setup_sleep_listener(struct sway_server *server) {
	struct sd_bus *bus;
	int ret = sd_bus_default_system(&bus);
	if (ret < 0) {
		wlr_log(L_ERROR, "Failed to open D-Bus connection: %s", strerror(-ret));
		return;
	}

	char str[256];
	const char *fmt = "type='signal',"
		"sender='org.freedesktop.login1',"
		"interface='org.freedesktop.login1.%s',"
		"member='%s',"
		"path='%s'";

	snprintf(str, sizeof(str), fmt, "Manager", "PrepareForSleep", "/org/freedesktop/login1");
	ret = sd_bus_add_match(bus, NULL, str, prepare_for_sleep, server);
	if (ret < 0) {
		wlr_log(L_ERROR, "Failed to add D-Bus match: %s", strerror(-ret));
		return;
	}
}
#endif

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
#if defined(WLR_HAS_SYSTEMD) || defined(WLR_HAS_ELOGIND)
			setup_sleep_listener(server);
#endif
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

