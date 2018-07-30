#ifndef _SWAYNAG_SWAYNAG_H
#define _SWAYNAG_SWAYNAG_H
#include <stdint.h>
#include <strings.h>
#include "list.h"
#include "pool-buffer.h"
#include "swaynag/types.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#define SWAYNAG_MAX_HEIGHT 500

enum swaynag_action_type {
	SWAYNAG_ACTION_DISMISS,
	SWAYNAG_ACTION_EXPAND,
	SWAYNAG_ACTION_COMMAND,
};

struct swaynag_pointer {
	struct wl_pointer *pointer;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor_image *cursor_image;
	struct wl_surface *cursor_surface;
	int x;
	int y;
};

struct swaynag_output {
	char *name;
	struct wl_output *wl_output;
	uint32_t wl_name;
};

struct swaynag_button {
	char *text;
	enum swaynag_action_type type;
	char *action;
	int x;
	int y;
	int width;
	int height;
};

struct swaynag_details {
	bool visible;
	char *message;

	int x;
	int y;
	int width;
	int height;

	int offset;
	int visible_lines;
	int total_lines;
	struct swaynag_button button_details;
	struct swaynag_button button_up;
	struct swaynag_button button_down;
};

struct swaynag {
	bool run_display;
	int querying_outputs;

	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_shm *shm;
	struct swaynag_pointer pointer;
	struct zxdg_output_manager_v1 *xdg_output_manager;
	struct swaynag_output output;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct wl_surface *surface;

	uint32_t width;
	uint32_t height;
	int32_t scale;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;

	struct swaynag_type *type;
	char *message;
	list_t *buttons;
	struct swaynag_details details;
};

void swaynag_setup(struct swaynag *swaynag);

void swaynag_run(struct swaynag *swaynag);

void swaynag_destroy(struct swaynag *swaynag);

#endif
