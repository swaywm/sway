#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output.h>
#include "sway/tree/view.h"

struct sway_server;
struct sway_container;

struct sway_output {
	struct wlr_output *wlr_output;
	struct sway_container *swayc;
	struct sway_server *server;

	struct wl_list layers[4]; // sway_layer_surface::link
	struct wlr_box usable_area;

	struct timespec last_frame;
	struct wlr_output_damage *damage;

	struct wl_listener destroy;
	struct wl_listener mode;
	struct wl_listener transform;
	struct wl_listener scale;

	struct wl_listener damage_destroy;
	struct wl_listener damage_frame;

	struct wl_list link;

	pid_t bg_pid;

	struct {
		struct wl_signal destroy;
	} events;
};

void output_damage_whole(struct sway_output *output);

void output_damage_surface(struct sway_output *output, double ox, double oy,
	struct wlr_surface *surface, bool whole);

void output_damage_from_view(struct sway_output *output,
	struct sway_view *view);

void output_damage_box(struct sway_output *output, struct wlr_box *box);

void output_damage_whole_container(struct sway_output *output,
	struct sway_container *con);

struct sway_container *output_by_name(const char *name);

void output_enable(struct sway_output *output);

bool output_has_opaque_lockscreen(struct sway_output *output,
		struct sway_seat *seat);

#endif
