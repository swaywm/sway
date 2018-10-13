#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include "list.h"
#include "log.h"
#include "loop.h"

struct loop_event {
	void (*callback)(int fd, short mask, void *data);
	void *data;
	bool is_timer;
};

struct loop {
	struct pollfd *fds;
	int fd_length;
	int fd_capacity;

	list_t *events; // struct loop_event
};

struct loop *loop_create(void) {
	struct loop *loop = calloc(1, sizeof(struct loop));
	if (!loop) {
		wlr_log(WLR_ERROR, "Unable to allocate memory for loop");
		return NULL;
	}
	loop->fd_capacity = 10;
	loop->fds = malloc(sizeof(struct pollfd) * loop->fd_capacity);
	loop->events = create_list();
	return loop;
}

void loop_destroy(struct loop *loop) {
	list_foreach(loop->events, free);
	list_free(loop->events);
	free(loop);
}

void loop_poll(struct loop *loop) {
	poll(loop->fds, loop->fd_length, -1);

	for (int i = 0; i < loop->fd_length; ++i) {
		struct pollfd pfd = loop->fds[i];
		struct loop_event *event = loop->events->items[i];

		// Always send these events
		unsigned events = pfd.events | POLLHUP | POLLERR;

		if (pfd.revents & events) {
			event->callback(pfd.fd, pfd.revents, event->data);

			if (event->is_timer) {
				loop_remove_event(loop, event);
				--i;
			}
		}
	}
}

struct loop_event *loop_add_fd(struct loop *loop, int fd, short mask,
		void (*callback)(int fd, short mask, void *data), void *data) {
	struct pollfd pfd = {fd, mask, 0};

	if (loop->fd_length == loop->fd_capacity) {
		loop->fd_capacity += 10;
		loop->fds = realloc(loop->fds, sizeof(struct pollfd) * loop->fd_capacity);
	}

	loop->fds[loop->fd_length++] = pfd;

	struct loop_event *event = calloc(1, sizeof(struct loop_event));
	event->callback = callback;
	event->data = data;

	list_add(loop->events, event);

	return event;
}

struct loop_event *loop_add_timer(struct loop *loop, int ms,
		void (*callback)(int fd, short mask, void *data), void *data) {
	int fd = timerfd_create(CLOCK_MONOTONIC, 0);
	struct itimerspec its;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = ms / 1000;
	its.it_value.tv_nsec = (ms % 1000) * 1000000;
	timerfd_settime(fd, 0, &its, NULL);

	struct loop_event *event = loop_add_fd(loop, fd, POLLIN, callback, data);
	event->is_timer = true;

	return event;
}

bool loop_remove_event(struct loop *loop, struct loop_event *event) {
	for (int i = 0; i < loop->events->length; ++i) {
		if (loop->events->items[i] == event) {
			list_del(loop->events, i);

			if (event->is_timer) {
				close(loop->fds[i].fd);
			}

			loop->fd_length--;
			memmove(&loop->fds[i], &loop->fds[i + 1], sizeof(void*) * (loop->fd_length - i));

			free(event);
			return true;
		}
	}
	return false;
}
