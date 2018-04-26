#ifndef _SWAY_CONFIG_H
#define _SWAY_CONFIG_H
#define PID_WORKSPACE_TIMEOUT 60
#include <libinput.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_box.h>
#include <xkbcommon/xkbcommon.h>
#include "list.h"
#include "tree/layout.h"
#include "tree/container.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

// TODO: Refactor this shit

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
	list_t *keysym_bindings;
	list_t *keycode_bindings;
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
	int drag_lock;
	int dwt;
	int left_handed;
	int middle_emulation;
	int natural_scroll;
	float pointer_accel;
	int repeat_delay;
	int repeat_rate;
	int scroll_method;
	int send_events;
	int tap;

	char *xkb_layout;
	char *xkb_model;
	char *xkb_options;
	char *xkb_rules;
	char *xkb_variant;

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

struct pid_workspace {
	pid_t *pid;
	char *workspace;
	time_t *time_added;
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
	E_BOTH,		/**< hide vertical and horizontal edge borders */
	E_SMART		/**< hide both if precisely one window is present in workspace */
};

enum command_context {
	CONTEXT_CONFIG = 1,
	CONTEXT_BINDING = 2,
	CONTEXT_IPC = 4,
	CONTEXT_CRITERIA = 8,
	CONTEXT_ALL = 0xFFFFFFFF,
};

struct command_policy {
	char *command;
	uint32_t context;
};

enum secure_feature {
	FEATURE_LOCK = 1,
	FEATURE_PANEL = 2,
	FEATURE_BACKGROUND = 4,
	FEATURE_SCREENSHOT = 8,
	FEATURE_FULLSCREEN = 16,
	FEATURE_KEYBOARD = 32,
	FEATURE_MOUSE = 64,
};

struct feature_policy {
	char *program;
	uint32_t features;
};

enum ipc_feature {
	IPC_FEATURE_COMMAND = 1,
	IPC_FEATURE_GET_WORKSPACES = 2,
	IPC_FEATURE_GET_OUTPUTS = 4,
	IPC_FEATURE_GET_TREE = 8,
	IPC_FEATURE_GET_MARKS = 16,
	IPC_FEATURE_GET_BAR_CONFIG = 32,
	IPC_FEATURE_GET_VERSION = 64,
	IPC_FEATURE_GET_INPUTS = 128,
	IPC_FEATURE_EVENT_WORKSPACE = 256,
	IPC_FEATURE_EVENT_OUTPUT = 512,
	IPC_FEATURE_EVENT_MODE = 1024,
	IPC_FEATURE_EVENT_WINDOW = 2048,
	IPC_FEATURE_EVENT_BINDING = 4096,
	IPC_FEATURE_EVENT_INPUT = 8192,
	IPC_FEATURE_GET_CLIPBOARD = 16384,

	IPC_FEATURE_ALL_COMMANDS = 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 16384,
	IPC_FEATURE_ALL_EVENTS = 256 | 512 | 1024 | 2048 | 4096 | 8192,

	IPC_FEATURE_ALL = IPC_FEATURE_ALL_COMMANDS | IPC_FEATURE_ALL_EVENTS,
};

struct ipc_policy {
	char *program;
	uint32_t features;
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
	list_t *pid_workspaces;
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
	uint32_t dragging_key;
	uint32_t resizing_key;
	char *floating_scroll_up_cmd;
	char *floating_scroll_down_cmd;
	char *floating_scroll_left_cmd;
	char *floating_scroll_right_cmd;
	enum sway_container_layout default_orientation;
	enum sway_container_layout default_layout;
	char *font;
	int font_height;

	// Flags
	bool focus_follows_mouse;
	bool mouse_warping;
	bool force_focus_wrapping;
	bool active;
	bool failed;
	bool reloading;
	bool reading;
	bool auto_back_and_forth;
	bool show_marks;

	bool edge_gaps;
	bool smart_gaps;
	int gaps_inner;
	int gaps_outer;

	list_t *config_chain;
	const char *current_config;

	enum sway_container_border border;
	enum sway_container_border floating_border;
	int border_thickness;
	int floating_border_thickness;
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

	// floating view
	int32_t floating_maximum_width;
	int32_t floating_maximum_height;
	int32_t floating_minimum_width;
	int32_t floating_minimum_height;

	// Security
	list_t *command_policies;
	list_t *feature_policies;
	list_t *ipc_policies;

	// Context for command handlers
	struct {
		struct input_config *input_config;
		struct seat_config *seat_config;
		struct sway_seat *seat;
		struct sway_container *current_container;
	} handler_context;
};

void pid_workspace_add(struct pid_workspace *pw);
void free_pid_workspace(struct pid_workspace *pw);

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

void config_clear_handler_context(struct sway_config *config);

void free_sway_variable(struct sway_variable *var);

/**
 * Does variable replacement for a string based on the config's currently loaded variables.
 */
char *do_var_replacement(char *str);

struct cmd_results *check_security_config();

int input_identifier_cmp(const void *item, const void *data);

struct input_config *new_input_config(const char* identifier);

void merge_input_config(struct input_config *dst, struct input_config *src);

struct input_config *copy_input_config(struct input_config *ic);

void free_input_config(struct input_config *ic);

void apply_input_config(struct input_config *input);

int seat_name_cmp(const void *item, const void *data);

struct seat_config *new_seat_config(const char* name);

void merge_seat_config(struct seat_config *dst, struct seat_config *src);

struct seat_config *copy_seat_config(struct seat_config *seat);

void free_seat_config(struct seat_config *ic);

struct seat_attachment_config *seat_attachment_config_new();

struct seat_attachment_config *seat_config_get_attachment(
		struct seat_config *seat_config, char *identifier);

void apply_seat_config(struct seat_config *seat);

int output_name_cmp(const void *item, const void *data);

void output_get_identifier(char *identifier, size_t len,
	struct sway_output *output);

struct output_config *new_output_config(const char *name);

void merge_output_config(struct output_config *dst, struct output_config *src);

void apply_output_config(struct output_config *oc,
		struct sway_container *output);

void free_output_config(struct output_config *oc);

int workspace_output_cmp_workspace(const void *a, const void *b);

int sway_binding_cmp(const void *a, const void *b);

int sway_binding_cmp_qsort(const void *a, const void *b);

int sway_binding_cmp_keys(const void *a, const void *b);

void free_sway_binding(struct sway_binding *sb);

struct sway_binding *sway_binding_dup(struct sway_binding *sb);

void load_swaybars();

void invoke_swaybar(struct bar_config *bar);

void terminate_swaybg(pid_t pid);

struct bar_config *default_bar_config(void);

void free_bar_config(struct bar_config *bar);

/* Global config singleton. */
extern struct sway_config *config;

/* Config file currently being read */
extern const char *current_config_path;

#endif
