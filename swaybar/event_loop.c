#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <poll.h>
#include "swaybar/bar.h"
#include "swaybar/event_loop.h"
#include "list.h"
#include "log.h"

struct event_item {
	void(*cb)(int fd, short mask, void *data);
	void *data;
};

struct timer_item {
	timer_t timer;
	void(*cb)(timer_t timer, void *data);
	void *data;
};

enum state_item_flags {
	ITEM_IS_FD,
	ITEM_IS_TIMER,
};

struct state_item {
	enum state_item_flags flags;
	union {
		int fd;
		timer_t timer;
	} inner;
};

static struct {
	// The order of each must be kept consistent
	struct {   /* pollfd array */
		struct pollfd *items;
		int capacity;
		int length;
	} fds;
	list_t *items; /* event_item list */

	// Timer list
	list_t *timers;

	// List of state changes at the end of each iteration
	list_t *state;
} event_loop;

void add_timer(timer_t timer,
		void(*cb)(timer_t timer, void *data),
		void *data) {

	struct timer_item *item = malloc(sizeof(struct timer_item));
	item->timer = timer;
	item->cb = cb;
	item->data = data;

	list_add(event_loop.timers, item);
}

void add_event(int fd, short mask,
		void(*cb)(int fd, short mask, void *data), void *data) {

	struct pollfd pollfd = {
		fd,
		mask,
		0,
	};

	// Resize
	if (event_loop.fds.length == event_loop.fds.capacity) {
		event_loop.fds.capacity += 10;
		event_loop.fds.items = realloc(event_loop.fds.items,
			sizeof(struct pollfd) * event_loop.fds.capacity);
	}

	event_loop.fds.items[event_loop.fds.length++] = pollfd;

	struct event_item *item = malloc(sizeof(struct event_item));
	item->cb = cb;
	item->data = data;

	list_add(event_loop.items, item);

	return;
}

static void _remove_event(int fd) {
	int index = -1;
	for (int i = 0; i < event_loop.fds.length; ++i) {
		if (event_loop.fds.items[i].fd == fd) {
			index = i;
		}
	}
	if (index != -1) {
		free(event_loop.items->items[index]);

		--event_loop.fds.length;
		memmove(&event_loop.fds.items[index], &event_loop.fds.items[index + 1],
				sizeof(struct pollfd) * event_loop.fds.length - index);

		list_del(event_loop.items, index);
	}
}

void remove_event(int fd) {
	struct state_item *item = malloc(sizeof(struct state_item));
	item->flags = ITEM_IS_FD;
	item->inner.fd = fd;
	list_add(event_loop.state, item);
}

static int timer_item_timer_cmp(const void *_timer_item, const void *_timer) {
	const struct timer_item *timer_item = _timer_item;
	const timer_t *timer = _timer;
	if (timer_item->timer == *timer) {
		return 0;
	} else {
		return -1;
	}
}
static void _remove_timer(timer_t timer) {
	int index = list_seq_find(event_loop.timers, timer_item_timer_cmp, &timer);
	if (index != -1) {
		free(event_loop.timers->items[index]);
		list_del(event_loop.timers, index);
	}
}

void remove_timer(timer_t timer) {
	struct state_item *item = malloc(sizeof(struct state_item));
	item->flags = ITEM_IS_TIMER;
	item->inner.timer = timer;
	list_add(event_loop.state, item);
}

void event_loop_poll() {
	poll(event_loop.fds.items, event_loop.fds.length, -1);

	for (int i = 0; i < event_loop.fds.length; ++i) {
		struct pollfd pfd = event_loop.fds.items[i];
		struct event_item *item = (struct event_item *)event_loop.items->items[i];

		if (pfd.revents & pfd.events) {
			item->cb(pfd.fd, pfd.revents, item->data);
		}
	}

	// check timers
	// not tested, but seems to work
	for (int i = 0; i < event_loop.timers->length; ++i) {
		struct timer_item *item = event_loop.timers->items[i];
		int overrun = timer_getoverrun(item->timer);
		if (overrun && overrun != -1) {
			item->cb(item->timer, item->data);
		}
	}

	// Remove all requested items from the event loop. We can't do this
	// during normal operation, as it will cause race conditions.
	for (int i = 0; i < event_loop.state->length; ++i) {
		struct state_item *item = event_loop.state->items[i];
		if (item->flags == ITEM_IS_FD) {
			_remove_event(item->inner.fd);
		} else {
			_remove_timer(item->inner.timer);
		}
		free(item);
	}
	event_loop.state->length = 0; // reset state list
}

void init_event_loop() {
	event_loop.fds.length = 0;
	event_loop.fds.capacity = 10;
	event_loop.fds.items = malloc(event_loop.fds.capacity * sizeof(struct pollfd));
	event_loop.items = create_list();
	event_loop.timers = create_list();
	event_loop.state = create_list();
}
