#ifndef _SWAY_CONFIG_H
#define _SWAY_CONFIG_H

#include <libinput.h>
#include <stdint.h>
#include <wlc/geometry.h>
#include <wlc/wlc.h>
#include <xkbcommon/xkbcommon.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "list.h"
#include "layout.h"
#include "container.h"

/**
 * Describes a variable created via the `set` command.
 */
struct sway_variable {
	char *name;
	char *value;
};

/**
 * A key binding and an associated command.
 */
struct sway_binding {
	int order;
	bool release;
	bool bindcode;
	list_t *keys;
	uint32_t modifiers;
	char *command;
};

/**
 * A mouse binding and an associated command.
 */
struct sway_mouse_binding {
	uint32_t button;
	char *command;
};

/**
 * A "mode" of keybindings created via the `mode` command.
 */
struct sway_mode {
	char *name;
	list_t *bindings;
};

/**
 * libinput options for input devices
 */
struct input_config {
	char *identifier;
	int click_method;
	int drag_lock;
	int dwt;
	int middle_emulation;
	int natural_scroll;
	float pointer_accel;
	int scroll_method;
	int send_events;
	int tap;

	bool capturable;
	struct wlc_geometry region;
};

/**
 * Size and position configuration for a particular output.
 *
 * This is set via the `output` command.
 */
struct output_config {
	char *name;
	int enabled;
	int width, height;
	int x, y;
	char *background;
	char *background_option;
};

/**
 * Maps a workspace name to an output name.
 *
 * Set via `workspace <x> output <y>`
 */
struct workspace_output {
	char *output;
	char *workspace;
};

struct bar_config {
	/**
	 * One of "dock", "hide", "invisible"
	 *
	 * Always visible in dock mode. Visible only when modifier key is held in hide mode.
	 * Never visible in invisible mode.
	 */
	char *mode;
	/**
	 * One of "show" or "hide".
	 *
	 * In "show" mode, it will always be shown on top of the active workspace.
	 */
	char *hidden_state;
	/**
	 * Id name used to identify the bar through IPC.
	 *
	 * Defaults to bar-x, where x corresponds to the position of the
	 * embedding bar block in the config file (bar-0, bar-1, ...).
	 */
	char *id;
	uint32_t modifier;
	list_t *outputs;
	enum desktop_shell_panel_position position;
	list_t *bindings;
	char *status_command;
	char *swaybar_command;
	char *font;
	int height; // -1 not defined
	int tray_padding;
	bool workspace_buttons;
	char *separator_symbol;
	bool strip_workspace_numbers;
	bool binding_mode_indicator;
	bool verbose;
	pid_t pid;
	struct {
		char background[10];
		char statusline[10];
		char separator[10];
		char focused_workspace_border[10];
		char focused_workspace_bg[10];
		char focused_workspace_text[10];
		char active_workspace_border[10];
		char active_workspace_bg[10];
		char active_workspace_text[10];
		char inactive_workspace_border[10];
		char inactive_workspace_bg[10];
		char inactive_workspace_text[10];
		char urgent_workspace_border[10];
		char urgent_workspace_bg[10];
		char urgent_workspace_text[10];
		char binding_mode_border[10];
		char binding_mode_bg[10];
		char binding_mode_text[10];
	} colors;
};

struct border_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
	uint32_t indicator;
	uint32_t child_border;
};

enum edge_border_types {
	E_NONE,         /**< Don't hide edge borders */
	E_VERTICAL,     /**< hide vertical edge borders */
	E_HORIZONTAL,   /**< hide horizontal edge borders */
	E_BOTH		/**< hide vertical and horizontal edge borders */
};

/**
 * The configuration struct. The result of loading a config file.
 */
struct sway_config {
	list_t *symbols;
	list_t *modes;
	list_t *bars;
	list_t *cmd_queue;
	list_t *workspace_outputs;
	list_t *output_configs;
	list_t *input_configs;
	list_t *criteria;
	list_t *active_bar_modifiers;
	struct sway_mode *current_mode;
	struct bar_config *current_bar;
	uint32_t floating_mod;
	uint32_t dragging_key;
	uint32_t resizing_key;
	enum swayc_layouts default_orientation;
	enum swayc_layouts default_layout;
	char *font;

	// Flags
	bool focus_follows_mouse;
	bool mouse_warping;
	bool active;
	bool failed;
	bool reloading;
	bool reading;
	bool auto_back_and_forth;
	bool seamless_mouse;

	bool edge_gaps;
	bool smart_gaps;
	int gaps_inner;
	int gaps_outer;

	list_t *config_chain;
	const char *current_config;

	enum swayc_border_types border;
	int border_thickness;
	enum edge_border_types hide_edge_borders;

	// border colors
	struct {
		struct border_colors focused;
		struct border_colors focused_inactive;
		struct border_colors unfocused;
		struct border_colors urgent;
		struct border_colors placeholder;
		uint32_t background;
	} border_colors;
};

/**
 * Loads the main config from the given path. is_active should be true when
 * reloading the config.
 */
bool load_main_config(const char *path, bool is_active);

/**
 * Loads an included config. Can only be used after load_main_config.
 */
bool load_include_configs(const char *path, struct sway_config *config);

/**
 * Reads the config from the given FILE.
 */
bool read_config(FILE *file, struct sway_config *config);

/**
 * Free config struct
 */
void free_config(struct sway_config *config);
/**
 * Does variable replacement for a string based on the config's currently loaded variables.
 */
char *do_var_replacement(char *str);

int input_identifier_cmp(const void *item, const void *data);
void merge_input_config(struct input_config *dst, struct input_config *src);
void apply_input_config(struct input_config *ic, struct libinput_device *dev);
void free_input_config(struct input_config *ic);

int output_name_cmp(const void *item, const void *data);
void merge_output_config(struct output_config *dst, struct output_config *src);
/** Sets up a WLC output handle based on a given output_config.
 */
void apply_output_config(struct output_config *oc, swayc_t *output);
void free_output_config(struct output_config *oc);

/**
 * Updates the list of active bar modifiers
 */
void update_active_bar_modifiers(void);

int workspace_output_cmp_workspace(const void *a, const void *b);

int sway_binding_cmp(const void *a, const void *b);
int sway_binding_cmp_qsort(const void *a, const void *b);
int sway_binding_cmp_keys(const void *a, const void *b);
void free_sway_binding(struct sway_binding *sb);
struct sway_binding *sway_binding_dup(struct sway_binding *sb);

int sway_mouse_binding_cmp(const void *a, const void *b);
int sway_mouse_binding_cmp_qsort(const void *a, const void *b);
int sway_mouse_binding_cmp_buttons(const void *a, const void *b);
void free_sway_mouse_binding(struct sway_mouse_binding *smb);

void load_swaybars();
void terminate_swaybg(pid_t pid);

/**
 * Allocate and initialize default bar configuration.
 */
struct bar_config *default_bar_config(void);

/**
 * Global config singleton.
 */
extern struct sway_config *config;

#endif
