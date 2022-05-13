#include <sys/resource.h>
#include <sched.h>
#include <unistd.h>
#include "sway/server.h"
#include "log.h"
#if HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif

#if HAVE_RTKIT

void gain_realtime(void) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;
	sd_bus *bus = NULL;
	uint32_t prio = sched_get_priority_min(SCHED_RR);
	int ret;
	struct rlimit rl;
	int max_usec;

	ret = sd_bus_open_system(&bus);
	if (ret < 0) {
		sway_log(SWAY_DEBUG, "Failed to connect to system bus");
		goto finish;
	}

	ret = sd_bus_get_property_trivial(bus, "org.freedesktop.RealtimeKit1",
								"/org/freedesktop/RealtimeKit1", "org.freedesktop.RealtimeKit1",
								"RTTimeUSecMax", &error, 'x', &max_usec);
	if (ret < 0) {
		sway_log(SWAY_DEBUG, "Couldn't query RTTimeUSecMax");
		goto finish;
	}

	rl.rlim_cur = max_usec;
	rl.rlim_max = max_usec;

	if (setrlimit(RLIMIT_RTTIME, &rl) < 0) {
		sway_log(SWAY_INFO, "Failed to setrlimit, no RT via RTKit possible");
		goto finish;
	}

	ret = sd_bus_call_method(bus, "org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1",
						"org.freedesktop.RealtimeKit1", "MakeThreadRealtime", &error, &m, "tu",
						(uint64_t)getpid(), prio);

	if (ret < 0) {
		sway_log(SWAY_ERROR, "Failed to setup SCHED_RR: %s", error.message);
		goto finish;
	}
	sway_log(SWAY_INFO, "Successfully setup SCHED_RR");

finish:
	sd_bus_error_free(&error);
	sd_bus_message_unref(m);
	sd_bus_unref(bus);
	return;
}

#else

void gain_realtime(void)
{
	return;
}

#endif
