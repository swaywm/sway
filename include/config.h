#ifndef _SWAY_CONFIG_H
#define _SWAY_CONFIG_H

#include <stdint.h>
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
	list_t *bindings;
	enum desktop_shell_panel_position position;
	char *status_command;
	char *font;
	int bar_height;
	int tray_padding;
	bool workspace_buttons;
	bool strip_workspace_numbers;
	bool binding_mode_indicator;
	bool verbose;
	struct {
		char *background;
		char *foreground;
		char *focused_workspace_border;
		char *focused_workspace_bg;
		char *focused_workspace_text;
		char *active_workspace_border;
		char *active_workspace_bg;
		char *active_workspace_text;
		char *inactive_workspace_border;
		char *inactive_workspace_bg;
		char *inactive_workspace_text;
	} colors;
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
	list_t *criteria;
	struct sway_mode *current_mode;
	struct bar_config bar;
	struct bar_config *current_bar;
	uint32_t floating_mod;
	enum swayc_layouts default_orientation;
	enum swayc_layouts default_layout;

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
	int gaps_inner;
	int gaps_outer;
};

/**
 * Loads the config from the given path.
 */
bool load_config(const char *file);
/** Reads the config from the given FILE.
 */
bool read_config(FILE *file, bool is_active);
/**
 * Does variable replacement for a string based on the config's currently loaded variables.
 */
char *do_var_replacement(char *str);
int output_name_cmp(const void *item, const void *data);
void merge_output_config(struct output_config *dst, struct output_config *src);
/** Sets up a WLC output handle based on a given output_config.
 */
void apply_output_config(struct output_config *oc, swayc_t *output);
void free_output_config(struct output_config *oc);

int workspace_output_cmp_workspace(const void *a, const void *b);

int sway_binding_cmp(const void *a, const void *b);
int sway_binding_cmp_keys(const void *a, const void *b);
void free_sway_binding(struct sway_binding *sb);

int sway_mouse_binding_cmp(const void *a, const void *b);
int sway_mouse_binding_cmp_buttons(const void *a, const void *b);
void free_sway_mouse_binding(struct sway_mouse_binding *smb);

/**
 * Global config singleton.
 */
extern struct sway_config *config;

#endif
