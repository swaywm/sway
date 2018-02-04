#ifndef _SWAYBAR_EVENT_LOOP_H
#define _SWAYBAR_EVENT_LOOP_H

#include <stdbool.h>
#include <time.h>

void add_event(int fd, short mask,
		void(*cb)(int fd, short mask, void *data),
		void *data);

// Not guaranteed to notify cb immediately
void add_timer(timer_t timer,
		void(*cb)(timer_t timer, void *data),
		void *data);

// Remove the given event from the event loop
void remove_event(int fd);

// Remove the given timer from the event loop
void remove_timer(timer_t timer);

// Blocks and returns after sending callbacks
void event_loop_poll();

void init_event_loop();
#endif /*_SWAYBAR_EVENT_LOOP_H */
