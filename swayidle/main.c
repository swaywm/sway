#define _XOPEN_SOURCE 500
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wlr/config.h>
#include <wlr/util/log.h>
#include "config.h"
#include "idle-client-protocol.h"
#include "list.h"
#ifdef SWAY_IDLE_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#elif defined(SWAY_IDLE_HAS_ELOGIND)
#include <elogind/sd-bus.h>
#include <elogind/sd-login.h>
#endif

typedef void (*timer_callback_func)(void *data);

static struct org_kde_kwin_idle *idle_manager = NULL;
static struct wl_seat *seat = NULL;
bool debug = false;

struct swayidle_state {
	struct wl_display *display;
	struct org_kde_kwin_idle_timeout *idle_timer;
	struct org_kde_kwin_idle_timeout *lock_timer;
	struct wl_event_loop *event_loop;
	list_t *timeout_cmds;
} state;

struct swayidle_cmd {
	timer_callback_func callback;
	char *param;
};

struct swayidle_cmd *lock_cmd = NULL;

struct swayidle_timeout_cmd {
	uint32_t timeout;
	struct swayidle_cmd *idle_cmd;
	struct swayidle_cmd *resume_cmd;
};

static void cmd_exec(void *data) {
	if (data == NULL) {
		return;
	}
	char *param = (char *)data;
	wlr_log(WLR_DEBUG, "Cmd exec %s", param);
	pid_t pid = fork();
	if (pid == 0) {
		pid = fork();
		if (pid == 0) {
			char *const cmd[] = { "sh", "-c", param, NULL, };
			execvp(cmd[0], cmd);
			wlr_log_errno(WLR_ERROR, "execve failed!");
			exit(1);
		} else if (pid < 0) {
			wlr_log_errno(WLR_ERROR, "fork failed");
			exit(1);
		}
		exit(0);
	} else if (pid < 0) {
		wlr_log_errno(WLR_ERROR, "fork failed");
	} else {
		wlr_log(WLR_DEBUG, "Spawned process %s", param);
		waitpid(pid, NULL, 0);
	}
}

#if defined(SWAY_IDLE_HAS_SYSTEMD) || defined(SWAY_IDLE_HAS_ELOGIND)
static int lock_fd = -1;
static int ongoing_fd = -1;

static int release_lock(void *data) {
	wlr_log(WLR_INFO, "Releasing sleep lock %d", ongoing_fd);
	if (ongoing_fd >= 0) {
		close(ongoing_fd);
	}
	ongoing_fd = -1;
	return 0;
}

void acquire_sleep_lock() {
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	struct sd_bus *bus;
	int ret = sd_bus_default_system(&bus);

	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to open D-Bus connection: %s",
				strerror(-ret));
		return;
	}

	ret = sd_bus_call_method(bus, "org.freedesktop.login1",
			"/org/freedesktop/login1",
			"org.freedesktop.login1.Manager", "Inhibit",
			&error, &msg, "ssss", "sleep", "swayidle",
			"Setup Up Lock Screen", "delay");
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to send Inhibit signal: %s",
				strerror(-ret));
	} else {
		ret = sd_bus_message_read(msg, "h", &lock_fd);
		if (ret < 0) {
			wlr_log(WLR_ERROR,
					"Failed to parse D-Bus response for Inhibit: %s",
					strerror(-ret));
		}
	}
	wlr_log(WLR_INFO, "Got sleep lock: %d", lock_fd);
}

static int prepare_for_sleep(sd_bus_message *msg, void *userdata,
		sd_bus_error *ret_error) {
	/* "b" apparently reads into an int, not a bool */
	int going_down = 1;
	int ret = sd_bus_message_read(msg, "b", &going_down);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to parse D-Bus response for Inhibit: %s",
				strerror(-ret));
	}
	wlr_log(WLR_DEBUG, "PrepareForSleep signal received %d", going_down);
	if (!going_down) {
		acquire_sleep_lock();
		return 0;
	}

	ongoing_fd = lock_fd;

	if (lock_cmd && lock_cmd->callback) {
		lock_cmd->callback(lock_cmd->param);
	}

	if (ongoing_fd >= 0) {
		struct wl_event_source *source =
			wl_event_loop_add_timer(state.event_loop, release_lock, NULL);
		wl_event_source_timer_update(source, 1000);
	}
	wlr_log(WLR_DEBUG, "Prepare for sleep done");
	return 0;
}

static int dbus_event(int fd, uint32_t mask, void *data) {
	sd_bus *bus = data;
	while (sd_bus_process(bus, NULL) > 0) {
		// Do nothing.
	}
	return 1;
}

void setup_sleep_listener() {
	struct sd_bus *bus;

	int ret = sd_bus_default_system(&bus);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to open D-Bus connection: %s",
				strerror(-ret));
		return;
	}

	char str[256];
	const char *fmt = "type='signal',"
		"sender='org.freedesktop.login1',"
		"interface='org.freedesktop.login1.%s',"
		"member='%s'," "path='%s'";

	snprintf(str, sizeof(str), fmt, "Manager", "PrepareForSleep",
			"/org/freedesktop/login1");
	ret = sd_bus_add_match(bus, NULL, str, prepare_for_sleep, NULL);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to add D-Bus match: %s", strerror(-ret));
		return;
	}
	acquire_sleep_lock();

	wl_event_loop_add_fd(state.event_loop, sd_bus_get_fd(bus),
			WL_EVENT_READABLE, dbus_event, bus);
}
#endif

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, org_kde_kwin_idle_interface.name) == 0) {
		idle_manager =
			wl_registry_bind(registry, name, &org_kde_kwin_idle_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void handle_idle(void *data, struct org_kde_kwin_idle_timeout *timer) {
	struct swayidle_timeout_cmd *cmd = data;
	wlr_log(WLR_DEBUG, "idle state");
	if (cmd && cmd->idle_cmd && cmd->idle_cmd->callback) {
		cmd->idle_cmd->callback(cmd->idle_cmd->param);
	}
}

static void handle_resume(void *data, struct org_kde_kwin_idle_timeout *timer) {
	struct swayidle_timeout_cmd *cmd = data;
	wlr_log(WLR_DEBUG, "active state");
	if (cmd && cmd->resume_cmd && cmd->resume_cmd->callback) {
		cmd->resume_cmd->callback(cmd->resume_cmd->param);
	}
}

static const struct org_kde_kwin_idle_timeout_listener idle_timer_listener = {
	.idle = handle_idle,
	.resumed = handle_resume,
};

struct swayidle_cmd *parse_command(int argc, char **argv) {
	if (argc < 1) {
		wlr_log(WLR_ERROR, "Too few parameters for command in parse_command");
		return NULL;
	}

	struct swayidle_cmd *cmd = calloc(1, sizeof(struct swayidle_cmd));
	wlr_log(WLR_DEBUG, "Command: %s", argv[0]);
	cmd->callback = cmd_exec;
	cmd->param = argv[0];
	return cmd;
}

int parse_timeout(int argc, char **argv) {
	if (argc < 3) {
		wlr_log(WLR_ERROR, "Too few parameters to timeout command. "
				"Usage: timeout <seconds> <command>");
		exit(-1);
	}
	errno = 0;
	char *endptr;
	int seconds = strtoul(argv[1], &endptr, 10);
	if (errno != 0 || *endptr != '\0') {
		wlr_log(WLR_ERROR, "Invalid timeout parameter '%s', it should be a "
				"numeric value representing seconds", optarg);
		exit(-1);
	}
	struct swayidle_timeout_cmd *cmd =
		calloc(1, sizeof(struct swayidle_timeout_cmd));
	cmd->timeout = seconds * 1000;

	wlr_log(WLR_DEBUG, "Register idle timeout at %d ms", cmd->timeout);
	wlr_log(WLR_DEBUG, "Setup idle");
	cmd->idle_cmd = parse_command(argc - 2, &argv[2]);

	int result = 3;
	if (argc >= 5 && !strcmp("resume", argv[3])) {
		wlr_log(WLR_DEBUG, "Setup resume");
		cmd->resume_cmd = parse_command(argc - 4, &argv[4]);
		result = 5;
	}
	list_add(state.timeout_cmds, cmd);
	return result;
}

int parse_sleep(int argc, char **argv) {
	if (argc < 2) {
		wlr_log(WLR_ERROR, "Too few parameters to before-sleep command. "
				"Usage: before-sleep <command>");
		exit(-1);
	}

	lock_cmd = parse_command(argc - 1, &argv[1]);
	if (lock_cmd) {
		wlr_log(WLR_DEBUG, "Setup sleep lock: %s", lock_cmd->param);
	}

	return 2;
}


int parse_args(int argc, char *argv[]) {
	int c;

	while ((c = getopt(argc, argv, "hs:d")) != -1) {
		switch(c) {
		case 'd':
			debug = true;
			break;
		case 'h':
		case '?':
			printf("Usage: %s [OPTIONS]\n", argv[0]);
			printf("  -d\tdebug\n");
			printf("  -h\tthis help menu\n");
			return 1;
		default:
			return 1;
		}
	}

	if (debug) {
		wlr_log_init(WLR_DEBUG, NULL);
		wlr_log(WLR_DEBUG, "Loglevel debug");
	} else {
		wlr_log_init(WLR_INFO, NULL);
	}


	state.timeout_cmds = create_list();

	int i = optind;
	while (i < argc) {
		if (!strcmp("timeout", argv[i])) {
			wlr_log(WLR_DEBUG, "Got timeout");
			i += parse_timeout(argc - i, &argv[i]);
		} else if (!strcmp("before-sleep", argv[i])) {
			wlr_log(WLR_DEBUG, "Got before-sleep");
			i += parse_sleep(argc - i, &argv[i]);
		} else {
			wlr_log(WLR_ERROR, "Unsupported command '%s'", argv[i]);
			exit(-1);
		}
	}
	return 0;
}

void sway_terminate(int exit_code) {
	if (state.event_loop) {
		wl_event_loop_destroy(state.event_loop);
	}
	if (state.display) {
		wl_display_disconnect(state.display);
	}
	exit(exit_code);
}

void sig_handler(int signal) {
	sway_terminate(0);
}

static int display_event(int fd, uint32_t mask, void *data) {
	if (mask & WL_EVENT_HANGUP) {
		sway_terminate(0);
	}
	if (wl_display_dispatch(state.display) < 0) {
		wlr_log_errno(WLR_ERROR, "wl_display_dispatch failed, exiting");
		sway_terminate(0);
	}
	return 0;
}

void register_idle_timeout(void *item) {
	struct swayidle_timeout_cmd *cmd = item;
	if (cmd == NULL || !cmd->timeout) {
		wlr_log(WLR_ERROR, "Invalid idle cmd, will not register");
		return;
	}
	state.idle_timer =
		org_kde_kwin_idle_get_idle_timeout(idle_manager, seat, cmd->timeout);
	if (state.idle_timer != NULL) {
		org_kde_kwin_idle_timeout_add_listener(state.idle_timer,
				&idle_timer_listener, cmd);
	} else {
		wlr_log(WLR_ERROR, "Could not create idle timer");
	}
}

int main(int argc, char *argv[]) {
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	if (parse_args(argc, argv) != 0) {
		return -1;
	}

	state.display = wl_display_connect(NULL);
	if (state.display == NULL) {
		wlr_log(WLR_ERROR, "Failed to create display");
		return -3;
	}

	struct wl_registry *registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(state.display);
	state.event_loop = wl_event_loop_create();

	if (idle_manager == NULL) {
		wlr_log(WLR_ERROR, "Display doesn't support idle protocol");
		return -4;
	}
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "Seat error");
		return -5;
	}

	bool should_run = state.timeout_cmds->length > 0;
#if defined(SWAY_IDLE_HAS_SYSTEMD) || defined(SWAY_IDLE_HAS_ELOGIND)
	if (lock_cmd) {
		should_run = true;
		setup_sleep_listener();
	}
#endif
	if (!should_run) {
		wlr_log(WLR_INFO, "No command specified! Nothing to do, will exit");
		sway_terminate(0);
	}
	list_foreach(state.timeout_cmds, register_idle_timeout);

	wl_display_roundtrip(state.display);

	wl_event_loop_add_fd(state.event_loop, wl_display_get_fd(state.display),
			WL_EVENT_READABLE, display_event, NULL);

	while (wl_event_loop_dispatch(state.event_loop, -1) != 1) {
		// This space intentionally left blank
	}

	sway_terminate(0);
}
