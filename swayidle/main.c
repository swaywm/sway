#define _POSIX_C_SOURCE 200809L
#include <assert.h>
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
#if HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#include <elogind/sd-login.h>
#endif

static struct org_kde_kwin_idle *idle_manager = NULL;
static struct wl_seat *seat = NULL;

struct swayidle_state {
	struct wl_display *display;
	struct wl_event_loop *event_loop;
	list_t *timeout_cmds; // struct swayidle_timeout_cmd *
	char *lock_cmd;
} state;

struct swayidle_timeout_cmd {
	int timeout, registered_timeout;
	struct org_kde_kwin_idle_timeout *idle_timer;
	char *idle_cmd;
	char *resume_cmd;
};

void sway_terminate(int exit_code) {
	wl_display_disconnect(state.display);
	wl_event_loop_destroy(state.event_loop);
	exit(exit_code);
}

static void cmd_exec(char *param) {
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

#if HAVE_SYSTEMD || HAVE_ELOGIND
static int lock_fd = -1;
static int ongoing_fd = -1;
static struct sd_bus *bus = NULL;

static int release_lock(void *data) {
	wlr_log(WLR_INFO, "Releasing sleep lock %d", ongoing_fd);
	if (ongoing_fd >= 0) {
		close(ongoing_fd);
	}
	ongoing_fd = -1;
	return 0;
}

static void acquire_sleep_lock(void) {
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret = sd_bus_call_method(bus, "org.freedesktop.login1",
			"/org/freedesktop/login1",
			"org.freedesktop.login1.Manager", "Inhibit",
			&error, &msg, "ssss", "sleep", "swayidle",
			"Setup Up Lock Screen", "delay");
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to send Inhibit signal: %s", error.message);
		sd_bus_error_free(&error);
		return;
	}

	ret = sd_bus_message_read(msg, "h", &lock_fd);
	if (ret < 0) {
		wlr_log(WLR_ERROR, "Failed to parse D-Bus response for Inhibit: %s",
			strerror(-ret));
	} else {
		wlr_log(WLR_INFO, "Got sleep lock: %d", lock_fd);
	}
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);
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

	if (state.lock_cmd) {
		cmd_exec(state.lock_cmd);
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

	if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
		sway_terminate(0);
	}

	int count = 0;
	if (mask & WL_EVENT_READABLE) {
		count = sd_bus_process(bus, NULL);
	}
	if (mask & WL_EVENT_WRITABLE) {
		sd_bus_flush(bus);
	}
	if (mask == 0) {
		sd_bus_flush(bus);
	}

	if (count < 0) {
		wlr_log_errno(WLR_ERROR, "sd_bus_process failed, exiting");
		sway_terminate(0);
	}

	return count;
}

static void setup_sleep_listener(void) {
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

	struct wl_event_source *source = wl_event_loop_add_fd(state.event_loop,
		sd_bus_get_fd(bus), WL_EVENT_READABLE, dbus_event, bus);
	wl_event_source_check(source);
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
	// Who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static const struct org_kde_kwin_idle_timeout_listener idle_timer_listener;

static void register_timeout(struct swayidle_timeout_cmd *cmd,
		int timeout) {
	if (cmd->idle_timer != NULL) {
		org_kde_kwin_idle_timeout_destroy(cmd->idle_timer);
		cmd->idle_timer = NULL;
	}
	if (timeout < 0) {
		wlr_log(WLR_DEBUG, "Not registering idle timeout");
		return;
	}
	wlr_log(WLR_DEBUG, "Register with timeout: %d", timeout);
	cmd->idle_timer =
		org_kde_kwin_idle_get_idle_timeout(idle_manager, seat, timeout);
	org_kde_kwin_idle_timeout_add_listener(cmd->idle_timer,
		&idle_timer_listener, cmd);
	cmd->registered_timeout = timeout;
}

static void handle_idle(void *data, struct org_kde_kwin_idle_timeout *timer) {
	struct swayidle_timeout_cmd *cmd = data;
	wlr_log(WLR_DEBUG, "idle state");
	if (cmd->idle_cmd) {
		cmd_exec(cmd->idle_cmd);
	}
}

static void handle_resume(void *data, struct org_kde_kwin_idle_timeout *timer) {
	struct swayidle_timeout_cmd *cmd = data;
	wlr_log(WLR_DEBUG, "active state");
	if (cmd->registered_timeout != cmd->timeout) {
		register_timeout(cmd, cmd->timeout);
	}
	if (cmd->resume_cmd) {
		cmd_exec(cmd->resume_cmd);
	}
}

static const struct org_kde_kwin_idle_timeout_listener idle_timer_listener = {
	.idle = handle_idle,
	.resumed = handle_resume,
};

static char *parse_command(int argc, char **argv) {
	if (argc < 1) {
		wlr_log(WLR_ERROR, "Missing command");
		return NULL;
	}

	wlr_log(WLR_DEBUG, "Command: %s", argv[0]);
	return strdup(argv[0]);
}

static int parse_timeout(int argc, char **argv) {
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

	if (seconds > 0) {
		cmd->timeout = seconds * 1000;
	} else {
		cmd->timeout = -1;
	}

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

static int parse_sleep(int argc, char **argv) {
	if (argc < 2) {
		wlr_log(WLR_ERROR, "Too few parameters to before-sleep command. "
				"Usage: before-sleep <command>");
		exit(-1);
	}

	state.lock_cmd = parse_command(argc - 1, &argv[1]);
	if (state.lock_cmd) {
		wlr_log(WLR_DEBUG, "Setup sleep lock: %s", state.lock_cmd);
	}

	return 2;
}

static int parse_args(int argc, char *argv[]) {
	bool debug = false;

	int c;
	while ((c = getopt(argc, argv, "hd")) != -1) {
		switch (c) {
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

	wlr_log_init(debug ? WLR_DEBUG : WLR_INFO, NULL);

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
			return 1;
		}
	}

	return 0;
}

static void register_zero_idle_timeout(void *item) {
	struct swayidle_timeout_cmd *cmd = item;
	register_timeout(cmd, 0);
}

static int handle_signal(int sig, void *data) {
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		sway_terminate(0);
		return 0;
	case SIGUSR1:
		wlr_log(WLR_DEBUG, "Got SIGUSR1");
		list_foreach(state.timeout_cmds, register_zero_idle_timeout);
		return 1;
	}
	assert(false); // not reached
}

static int display_event(int fd, uint32_t mask, void *data) {
	if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
		sway_terminate(0);
	}

	int count = 0;
	if (mask & WL_EVENT_READABLE) {
		count = wl_display_dispatch(state.display);
	}
	if (mask & WL_EVENT_WRITABLE) {
		wl_display_flush(state.display);
	}
	if (mask == 0) {
		count = wl_display_dispatch_pending(state.display);
		wl_display_flush(state.display);
	}

	if (count < 0) {
		wlr_log_errno(WLR_ERROR, "wl_display_dispatch failed, exiting");
		sway_terminate(0);
	}

	return count;
}

static void register_idle_timeout(void *item) {
	struct swayidle_timeout_cmd *cmd = item;
	register_timeout(cmd, cmd->timeout);
}

int main(int argc, char *argv[]) {
	if (parse_args(argc, argv) != 0) {
		return -1;
	}

	state.event_loop = wl_event_loop_create();

	wl_event_loop_add_signal(state.event_loop, SIGINT, handle_signal, NULL);
	wl_event_loop_add_signal(state.event_loop, SIGTERM, handle_signal, NULL);
	wl_event_loop_add_signal(state.event_loop, SIGUSR1, handle_signal, NULL);

	state.display = wl_display_connect(NULL);
	if (state.display == NULL) {
		wlr_log(WLR_ERROR, "Unable to connect to the compositor. "
				"If your compositor is running, check or set the "
				"WAYLAND_DISPLAY environment variable.");
		return -3;
	}

	struct wl_registry *registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(state.display);

	if (idle_manager == NULL) {
		wlr_log(WLR_ERROR, "Display doesn't support idle protocol");
		return -4;
	}
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "Seat error");
		return -5;
	}

	bool should_run = state.timeout_cmds->length > 0;
#if HAVE_SYSTEMD || HAVE_ELOGIND
	if (state.lock_cmd) {
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

	struct wl_event_source *source = wl_event_loop_add_fd(state.event_loop,
		wl_display_get_fd(state.display), WL_EVENT_READABLE,
		display_event, NULL);
	wl_event_source_check(source);

	while (wl_event_loop_dispatch(state.event_loop, -1) != 1) {
		// This space intentionally left blank
	}

	sway_terminate(0);
}
