#ifndef _SWAY_NAGBAR_NAGBAR_H
#define _SWAY_NAGBAR_NAGNAR_H
#include <stdint.h>
#include "list.h"
#include "pool-buffer.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#define NAGBAR_BAR_BORDER_THICKNESS 2
#define NAGBAR_MESSAGE_PADDING 8
#define NAGBAR_DETAILS_BORDER_THICKNESS 3
#define NAGBAR_BUTTON_BORDER_THICKNESS 3
#define NAGBAR_BUTTON_GAP 20
#define NAGBAR_BUTTON_GAP_CLOSE 15
#define NAGBAR_BUTTON_MARGIN_RIGHT 2
#define NAGBAR_BUTTON_PADDING 3

#define NAGBAR_MAX_HEIGHT 500

enum sway_nagbar_type {
	NAGBAR_ERROR,
	NAGBAR_WARNING,
};

enum sway_nagbar_action_type {
	NAGBAR_ACTION_DISMISS,
	NAGBAR_ACTION_EXPAND,
	NAGBAR_ACTION_COMMAND,
};

struct sway_nagbar_colors {
	uint32_t button_background;
	uint32_t background;
	uint32_t text;
	uint32_t border;
	uint32_t border_bottom;
};

struct sway_nagbar_pointer {
	struct wl_pointer *pointer;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor_image *cursor_image;
	struct wl_surface *cursor_surface;
	int x;
	int y;
};

struct sway_nagbar_output {
	char *name;
	struct wl_output *wl_output;
	uint32_t wl_name;
};

struct sway_nagbar_button {
	char *text;
	enum sway_nagbar_action_type type;
	char *action;
	int x;
	int y;
	int width;
	int height;
};

struct sway_nagbar_details {
	bool visible;
	char *message;

	int x;
	int y;
	int width;
	int height;

	int offset;
	int visible_lines;
	int total_lines;
	struct sway_nagbar_button button_up;
	struct sway_nagbar_button button_down;
};

struct sway_nagbar {
	bool run_display;
	int querying_outputs;

	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_shm *shm;
	struct sway_nagbar_pointer pointer;
	struct zxdg_output_manager_v1 *xdg_output_manager;
	struct sway_nagbar_output output;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct wl_surface *surface;

	uint32_t width;
	uint32_t height;
	int32_t scale;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;

	enum sway_nagbar_type type;
	struct sway_nagbar_colors colors;
	uint32_t anchors;
	char *message;
	char *font;
	list_t *buttons;
	struct sway_nagbar_details details;
};

void nagbar_setup(struct sway_nagbar *nagbar);

void nagbar_run(struct sway_nagbar *nagbar);

void nagbar_destroy(struct sway_nagbar *nagbar);

#endif
