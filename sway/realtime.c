#include <sys/resource.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include "sway/server.h"
#include "log.h"

static void child_fork_callback(void) {
	struct sched_param param;

	param.sched_priority = 0;

	int ret = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
	if (ret != 0) {
		sway_log(SWAY_ERROR, "Failed to reset scheduler policy on fork");
	}
}

void set_rr_scheduling(void) {
	int prio = sched_get_priority_min(SCHED_RR);
	int old_policy;
	int ret;
	struct sched_param param;

	ret = pthread_getschedparam(pthread_self(), &old_policy, &param);
	if (ret != 0) {
		sway_log(SWAY_DEBUG, "Failed to get old scheduling priority");
		return;
	}

	param.sched_priority = prio;

	ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
	if (ret != 0) {
		sway_log(SWAY_INFO, "Failed to set scheduling priority to %d", prio);
		return;
	}

	pthread_atfork(NULL, NULL, child_fork_callback);
}
