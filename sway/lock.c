#define _POSIX_C_SOURCE 200809L
#include <drm_fourcc.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/types/wlr_output_damage.h>
#include "cairo_util.h"
#include "log.h"
#include "pango.h"
#include "sway/input/seat.h"
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/server.h"
#include "util.h"

#define PERMALOCK_CLIENT (struct wl_client *)(-1)

static struct wlr_texture *draw_permalock_message(void) {
	sway_log(SWAY_DEBUG, "CREATING PERMALOCK MESSAGE");
	struct sway_output *output = root->outputs->items[0];

	int scale = output->wlr_output->scale;
	int width = 0;
	int height = 0;

	const char* permalock_msg = "Lock screen crashed. Start a new lockscreen to unlock.";

	// We must use a non-nil cairo_t for cairo_set_font_options to work.
	// Therefore, we cannot use cairo_create(NULL).
	cairo_surface_t *dummy_surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, 0, 0);
	cairo_t *c = cairo_create(dummy_surface);
	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_NONE);
	cairo_set_font_options(c, fo);
	get_text_size(c, config->font, &width, &height, NULL, scale,
			config->pango_markup, "%s", permalock_msg);
	cairo_surface_destroy(dummy_surface);
	cairo_destroy(c);

	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
	cairo_set_source_rgba(cairo, 1.0,1.0,1.0,0.0);
	cairo_paint(cairo);
	PangoContext *pango = pango_cairo_create_context(cairo);
	cairo_set_source_rgba(cairo, 0.,0.,0.,1.0);
	cairo_move_to(cairo, 0, 0);

	pango_printf(cairo, config->font, scale, config->pango_markup,
			"%s", permalock_msg);

	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
			output->wlr_output->backend);
	struct wlr_texture *tex = wlr_texture_from_pixels(
			renderer, DRM_FORMAT_ARGB8888, stride, width, height, data);
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
	return tex;
}

struct lock_finished_delay;
struct lock_finished_item {
	struct wl_listener listener;
	struct lock_finished_delay *delay;
};

struct lock_finished_delay {
	struct wl_listener canceller;
	int nr_outputs;
	int nr_pending;
	struct lock_finished_item items[0];
};

static void lock_finished_cleanup(struct lock_finished_delay *delay) {
	for (int i = 0; i < delay->nr_outputs; ++i) {
		if (delay->items[i].listener.notify) {
			wl_list_remove(&delay->items[i].listener.link);
		}
	}
	wl_list_remove(&delay->canceller.link);
	free(delay);
}

static void lock_finished_frame(struct wl_listener *listener, void *data) {
	struct lock_finished_item *item = wl_container_of(listener, item, listener);
	struct lock_finished_delay *delay = item->delay;
	wl_list_remove(&listener->link);
	listener->notify = NULL;
	delay->nr_pending--;
	if (delay->nr_pending == 0) {
		wlr_screenlock_send_lock_finished(server.screenlock);
		lock_finished_cleanup(delay);
	}
}

static void lock_finished_abort(struct wl_listener *listener, void *data) {
	struct wlr_screenlock_change *signal = data;
	if (signal->how == WLR_SCREENLOCK_MODE_CHANGE_UNLOCK) {
		struct lock_finished_delay *delay = wl_container_of(listener, delay, canceller);
		lock_finished_cleanup(delay);
	}
	// any other 'how' means we should continue
}

void handle_lock_set_mode(struct wl_listener *listener, void *data)
{
	struct wlr_screenlock_change *signal = data;
	struct wl_client *client = signal->new_client;
	struct sway_seat *seat;
	int nr_outputs = root->outputs->length;

	switch (signal->how) {
	case WLR_SCREENLOCK_MODE_CHANGE_LOCK:
		wl_list_for_each(seat, &server.input->seats, link) {
			seat_set_exclusive_client(seat, client);
		}

		// delay lock_finished until the frame is displayed on all enabled displays
		struct lock_finished_delay *delay = calloc(1, sizeof(*delay) + nr_outputs * sizeof(delay->items[0]));
		delay->nr_outputs = nr_outputs;

		for (int i = 0; i < nr_outputs; ++i) {
			struct sway_output *output = root->outputs->items[i];
			if (output && output->enabled && output->wlr_output && output->damage) {
				wl_signal_add(&output->damage->events.frame, &delay->items[i].listener);
				delay->items[i].listener.notify = lock_finished_frame;
				delay->items[i].delay = delay;
				delay->nr_pending++;
			}
		}

		if (delay->nr_pending) {
			wl_signal_add(&server.screenlock->events.change_request,
				&delay->canceller);
			delay->canceller.notify = lock_finished_abort;
		} else {
			wlr_screenlock_send_lock_finished(server.screenlock);
			free(delay);
		}

		break;

	case WLR_SCREENLOCK_MODE_CHANGE_REPLACE:
	case WLR_SCREENLOCK_MODE_CHANGE_RESPAWN:
		wl_list_for_each(seat, &server.input->seats, link) {
			seat_set_exclusive_client(seat, client);
		}

		wlr_screenlock_send_lock_finished(server.screenlock);
		break;

	case WLR_SCREENLOCK_MODE_CHANGE_UNLOCK:
		wl_list_for_each(seat, &server.input->seats, link) {
			seat_set_exclusive_client(seat, NULL);
			// copied from input_manager -- deduplicate?
			struct sway_node *previous = seat_get_focus(seat);
			if (previous) {
				// Hack to get seat to re-focus the return value of get_focus
				seat_set_focus(seat, NULL);
				seat_set_focus(seat, previous);
			}
		}
		// No animation, send finished now
		wlr_screenlock_send_unlock_finished(server.screenlock);
		break;

	case WLR_SCREENLOCK_MODE_CHANGE_ABANDON:
		sway_log(SWAY_ERROR, "Lockscreen client died, showing fallback message");
		wl_list_for_each(seat, &server.input->seats, link) {
			seat_set_exclusive_client(seat, PERMALOCK_CLIENT);
		}
		if (!server.permalock_message) {
			server.permalock_message = draw_permalock_message();
		}
		break;
	}

	// redraw everything
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_whole(output);
	}
}
