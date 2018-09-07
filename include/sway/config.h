#ifndef _SWAY_CONFIG_H
#define _SWAY_CONFIG_H
#include <libinput.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_box.h>
#include <xkbcommon/xkbcommon.h>
#include "list.h"
#include "swaynag.h"
#include "tree/container.h"
#include "sway/tree/root.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

// TODO: Refactor this shit

/**
 * Describes a variable created via the `set` command.
 */
struct sway_variable {
	char *name;
	char *value;
};


enum binding_input_type {
	BINDING_KEYCODE,
	BINDING_KEYSYM,
	BINDING_MOUSE,
};

enum binding_flags {
	BINDING_RELEASE=1,
	BINDING_LOCKED=2,    // keyboard only
	BINDING_BORDER=4,    // mouse only; trigger on container border
	BINDING_CONTENTS=8,  // mouse only; trigger on container contents
	BINDING_TITLEBAR=16, // mouse only; trigger on container titlebar
};

/**
 * A key binding and an associated command.
 */
struct sway_binding {
	enum binding_input_type type;
	int order;
	uint32_t flags;
	list_t *keys; // sorted in ascending order
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
 * Focus on window activation.
 */
enum sway_fowa {
	FOWA_SMART,
	FOWA_URGENT,
	FOWA_FOCUS,
	FOWA_NONE,
};

/**
 * A "mode" of keybindings created via the `mode` command.
 */
struct sway_mode {
	char *name;
	list_t *keysym_bindings;
	list_t *keycode_bindings;
	list_t *mouse_bindings;
	bool pango;
};

struct input_config_mapped_from_region {
	double x1, y1;
	double x2, y2;
	bool mm;
};

/**
 * options for input devices
 */
struct input_config {
	char *identifier;

	int accel_profile;
	int click_method;
	int drag;
	int drag_lock;
	int dwt;
	int left_handed;
	int middle_emulation;
	int natural_scroll;
	float pointer_accel;
	int repeat_delay;
	int repeat_rate;
	int scroll_button;
	int scroll_method;
	int send_events;
	int tap;
	int tap_button_map;

	char *xkb_layout;
	char *xkb_model;
	char *xkb_options;
	char *xkb_rules;
	char *xkb_variant;

	int xkb_numlock;
	int xkb_capslock;

	struct input_config_mapped_from_region *mapped_from_region;
	char *mapped_to_output;

	bool capturable;
	struct wlr_box region;
};

/**
 * Options for misc device configurations that happen in the seat block
 */
struct seat_attachment_config {
	char *identifier;
	// TODO other things are configured here for some reason
};

/**
 * Options for multiseat and other misc device configurations
 */
struct seat_config {
	char *name;
	int fallback; // -1 means not set
	list_t *attachments; // list of seat_attachment configs
};

enum config_dpms {
	DPMS_IGNORE,
	DPMS_ON,
	DPMS_OFF
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
	float refresh_rate;
	int x, y;
	float scale;
	int32_t transform;

	char *background;
	char *background_option;
	char *background_fallback;
	enum config_dpms dpms_state;
};

/**
 * Stores configuration for a workspace, regardless of whether the workspace
 * exists.
 */
struct workspace_config {
	char *workspace;
	char *output;
	int gaps_inner;
	int gaps_outer;
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
	char *position;
	list_t *bindings;
	char *status_command;
	bool pango_markup;
	char *swaybar_command;
	char *font;
	int height; // -1 not defined
	bool workspace_buttons;
	bool wrap_scroll;
	char *separator_symbol;
	bool strip_workspace_numbers;
	bool binding_mode_indicator;
	bool verbose;
	pid_t pid;
	struct {
		char *background;
		char *statusline;
		char *separator;
		char *focused_background;
		char *focused_statusline;
		char *focused_separator;
		char *focused_workspace_border;
		char *focused_workspace_bg;
		char *focused_workspace_text;
		char *active_workspace_border;
		char *active_workspace_bg;
		char *active_workspace_text;
		char *inactive_workspace_border;
		char *inactive_workspace_bg;
		char *inactive_workspace_text;
		char *urgent_workspace_border;
		char *urgent_workspace_bg;
		char *urgent_workspace_text;
		char *binding_mode_border;
		char *binding_mode_bg;
		char *binding_mode_text;
	} colors;
};

struct border_colors {
	float border[4];
	float background[4];
	float text[4];
	float indicator[4];
	float child_border[4];
};

enum edge_border_types {
	E_NONE,         /**< Don't hide edge borders */
	E_VERTICAL,     /**< hide vertical edge borders */
	E_HORIZONTAL,   /**< hide horizontal edge borders */
	E_BOTH,		/**< hide vertical and horizontal edge borders */
	E_SMART, /**< hide both if precisely one window is present in workspace */
	E_SMART_NO_GAPS, /**< hide both if one window and gaps to edge is zero */
};

enum sway_popup_during_fullscreen {
	POPUP_SMART,
	POPUP_IGNORE,
	POPUP_LEAVE,
};

enum secure_feature {
	FEATURE_FULLSCREEN       = 1 << 0,
	FEATURE_DATA_CONTROL_MGR = 1 << 1,
	FEATURE_DMABUF_EXPORT    = 1 << 2,
	FEATURE_SCREENCOPY       = 1 << 3,
	FEATURE_GAMMA_CONTROL    = 1 << 4,
	FEATURE_INPUT_INHIBIT    = 1 << 5,
	FEATURE_LAYER_SHELL      = 1 << 6,
	FEATURE_VIRTUAL_KEYBOARD = 1 << 7,
};

struct feature_policy {
	char *program;
	uint64_t permit_features;
	uint64_t reject_features;
};

enum focus_wrapping_mode {
	WRAP_NO,
	WRAP_YES,
	WRAP_FORCE
};

/**
 * The configuration struct. The result of loading a config file.
 */
struct sway_config {
	char *swaynag_command;
	struct swaynag_instance swaynag_config_errors;
	list_t *symbols;
	list_t *modes;
	list_t *bars;
	list_t *cmd_queue;
	list_t *workspace_configs;
	list_t *output_configs;
	list_t *input_configs;
	list_t *seat_configs;
	list_t *criteria;
	list_t *no_focus;
	list_t *active_bar_modifiers;
	struct sway_mode *current_mode;
	struct bar_config *current_bar;
	char *swaybg_command;
	uint32_t floating_mod;
	bool floating_mod_inverse;
	uint32_t dragging_key;
	uint32_t resizing_key;
	char *floating_scroll_up_cmd;
	char *floating_scroll_down_cmd;
	char *floating_scroll_left_cmd;
	char *floating_scroll_right_cmd;
	enum sway_container_layout default_orientation;
	enum sway_container_layout default_layout;
	char *font;
	size_t font_height;
	size_t font_baseline;
	bool pango_markup;
	size_t urgent_timeout;
	enum sway_fowa focus_on_window_activation;
	enum sway_popup_during_fullscreen popup_during_fullscreen;

	// Flags
	bool focus_follows_mouse;
	bool raise_floating;
	bool mouse_warping;
	enum focus_wrapping_mode focus_wrapping;
	bool active;
	bool failed;
	bool reading;
	bool reloading;
	bool secure;
	bool validating;
	bool auto_back_and_forth;
	bool show_marks;
	bool tiling_drag;

	bool edge_gaps;
	bool smart_gaps;
	int gaps_inner;
	int gaps_outer;

	list_t *config_chain;
	const char *current_config_path;
	const char *current_config;

	enum sway_container_border border;
	enum sway_container_border floating_border;
	int border_thickness;
	int floating_border_thickness;
	enum edge_border_types hide_edge_borders;
	enum edge_border_types saved_edge_borders;

	// border colors
	struct {
		struct border_colors focused;
		struct border_colors focused_inactive;
		struct border_colors unfocused;
		struct border_colors urgent;
		struct border_colors placeholder;
		float background[4];
	} border_colors;

	// floating view
	int32_t floating_maximum_width;
	int32_t floating_maximum_height;
	int32_t floating_minimum_width;
	int32_t floating_minimum_height;

	// Security
	list_t *feature_policies; // struct feature_policy
	struct feature_policy default_policy;

	// Context for command handlers
	struct {
		struct input_config *input_config;
		struct output_config *output_config;
		struct seat_config *seat_config;
		struct sway_seat *seat;
		struct sway_node *node;
		struct sway_container *container;
		struct sway_workspace *workspace;
		bool using_criteria;
		struct {
			int argc;
			char **argv;
		} leftovers;
	} handler_context;
};

/**
 * Loads the main config from the given path. is_active should be true when
 * reloading the config.
 */
bool load_main_config(const char *path, bool is_active, bool validating);

/**
 * Loads an included config. Can only be used after load_main_config.
 */
bool load_include_configs(const char *path, struct sway_config *config,
		struct swaynag_instance *swaynag);

/**
 * Reads the config from the given FILE.
 */
bool read_config(FILE *file, struct sway_config *config,
		struct swaynag_instance *swaynag);

/**
 * Free config struct
 */
void free_config(struct sway_config *config);

void free_sway_variable(struct sway_variable *var);

/**
 * Does variable replacement for a string based on the config's currently loaded variables.
 */
char *do_var_replacement(char *str);

struct cmd_results *check_security_config(struct sway_config *config);

int input_identifier_cmp(const void *item, const void *data);

struct input_config *new_input_config(const char* identifier);

void merge_input_config(struct input_config *dst, struct input_config *src);

struct input_config *store_input_config(struct input_config *ic);

void free_input_config(struct input_config *ic);

int seat_name_cmp(const void *item, const void *data);

struct seat_config *new_seat_config(const char* name);

void merge_seat_config(struct seat_config *dst, struct seat_config *src);

struct seat_config *copy_seat_config(struct seat_config *seat);

void free_seat_config(struct seat_config *ic);

struct seat_attachment_config *seat_attachment_config_new(void);

struct seat_attachment_config *seat_config_get_attachment(
		struct seat_config *seat_config, char *identifier);

void apply_seat_config(struct seat_config *seat);

int output_name_cmp(const void *item, const void *data);

void output_get_identifier(char *identifier, size_t len,
	struct sway_output *output);

struct output_config *new_output_config(const char *name);

void merge_output_config(struct output_config *dst, struct output_config *src);

void apply_output_config(struct output_config *oc, struct sway_output *output);

struct output_config *store_output_config(struct output_config *oc);

void apply_output_config_to_outputs(struct output_config *oc);

void free_output_config(struct output_config *oc);

void create_default_output_configs(void);

int workspace_output_cmp_workspace(const void *a, const void *b);

int sway_binding_cmp(const void *a, const void *b);

int sway_binding_cmp_qsort(const void *a, const void *b);

int sway_binding_cmp_keys(const void *a, const void *b);

void free_sway_binding(struct sway_binding *sb);

void seat_execute_command(struct sway_seat *seat, struct sway_binding *binding);

void load_swaybars(void);

void terminate_swaybg(pid_t pid);

struct bar_config *default_bar_config(void);

void free_bar_config(struct bar_config *bar);

void free_workspace_config(struct workspace_config *wsc);

/**
 * Updates the value of config->font_height based on the max title height
 * reported by each container. If recalculate is true, the containers will
 * recalculate their heights before reporting.
 *
 * If the height has changed, all containers will be rearranged to take on the
 * new size.
 */
void config_update_font_height(bool recalculate);

/* Global config singleton. */
extern struct sway_config *config;

#endif
