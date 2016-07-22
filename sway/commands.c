#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <wlc/wlc.h>
#include <wlc/wlc-render.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <wordexp.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <float.h>
#include <libinput.h>
#include "stringop.h"
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "util.h"
#include "workspace.h"
#include "commands.h"
#include "container.h"
#include "output.h"
#include "handlers.h"
#include "sway.h"
#include "resize.h"
#include "input_state.h"
#include "criteria.h"
#include "ipc-server.h"
#include "list.h"
#include "input.h"
#include "border.h"

typedef struct cmd_results *sway_cmd(int argc, char **argv);

struct cmd_handler {
	char *command;
	sway_cmd *handle;
};

static sway_cmd cmd_assign;
static sway_cmd cmd_bar;
static sway_cmd cmd_bindcode;
static sway_cmd cmd_bindsym;
static sway_cmd cmd_border;
static sway_cmd cmd_client_focused;
static sway_cmd cmd_client_focused_inactive;
static sway_cmd cmd_client_unfocused;
static sway_cmd cmd_client_urgent;
static sway_cmd cmd_client_placeholder;
static sway_cmd cmd_client_background;
static sway_cmd cmd_debuglog;
static sway_cmd cmd_exec;
static sway_cmd cmd_exec_always;
static sway_cmd cmd_exit;
static sway_cmd cmd_floating;
static sway_cmd cmd_floating_maximum_size;
static sway_cmd cmd_floating_minimum_size;
static sway_cmd cmd_floating_mod;
static sway_cmd cmd_floating_scroll;
static sway_cmd cmd_focus;
static sway_cmd cmd_focus_follows_mouse;
static sway_cmd cmd_font;
static sway_cmd cmd_for_window;
static sway_cmd cmd_fullscreen;
static sway_cmd cmd_gaps;
static sway_cmd cmd_hide_edge_borders;
static sway_cmd cmd_include;
static sway_cmd cmd_input;
static sway_cmd cmd_kill;
static sway_cmd cmd_layout;
static sway_cmd cmd_log_colors;
static sway_cmd cmd_mode;
static sway_cmd cmd_mouse_warping;
static sway_cmd cmd_move;
static sway_cmd cmd_new_float;
static sway_cmd cmd_new_window;
static sway_cmd cmd_orientation;
static sway_cmd cmd_output;
static sway_cmd cmd_reload;
static sway_cmd cmd_resize;
static sway_cmd cmd_resize_set;
static sway_cmd cmd_scratchpad;
static sway_cmd cmd_set;
static sway_cmd cmd_smart_gaps;
static sway_cmd cmd_split;
static sway_cmd cmd_splith;
static sway_cmd cmd_splitt;
static sway_cmd cmd_splitv;
static sway_cmd cmd_sticky;
static sway_cmd cmd_workspace;
static sway_cmd cmd_ws_auto_back_and_forth;
static sway_cmd cmd_workspace_layout;

static sway_cmd bar_cmd_binding_mode_indicator;
static sway_cmd bar_cmd_bindsym;
static sway_cmd bar_cmd_colors;
static sway_cmd bar_cmd_font;
static sway_cmd bar_cmd_mode;
static sway_cmd bar_cmd_modifier;
static sway_cmd bar_cmd_output;
static sway_cmd bar_cmd_height;
static sway_cmd bar_cmd_hidden_state;
static sway_cmd bar_cmd_id;
static sway_cmd bar_cmd_position;
static sway_cmd bar_cmd_separator_symbol;
static sway_cmd bar_cmd_status_command;
static sway_cmd bar_cmd_pango_markup;
static sway_cmd bar_cmd_strip_workspace_numbers;
static sway_cmd bar_cmd_swaybar_command;
static sway_cmd bar_cmd_tray_output;
static sway_cmd bar_cmd_tray_padding;
static sway_cmd bar_cmd_wrap_scroll;
static sway_cmd bar_cmd_workspace_buttons;

static sway_cmd bar_colors_cmd_active_workspace;
static sway_cmd bar_colors_cmd_background;
static sway_cmd bar_colors_cmd_background;
static sway_cmd bar_colors_cmd_binding_mode;
static sway_cmd bar_colors_cmd_focused_workspace;
static sway_cmd bar_colors_cmd_inactive_workspace;
static sway_cmd bar_colors_cmd_separator;
static sway_cmd bar_colors_cmd_statusline;
static sway_cmd bar_colors_cmd_urgent_workspace;

static struct cmd_results *add_color(const char*, char*, const char*);

swayc_t *sp_view;
int sp_index = 0;

static char *bg_options[] = {
	"stretch",
	"center",
	"fill",
	"fit",
	"tile"
};

enum expected_args {
	EXPECTED_MORE_THAN,
	EXPECTED_AT_LEAST,
	EXPECTED_LESS_THAN,
	EXPECTED_EQUAL_TO
};

// Returns error object, or NULL if check succeeds.
static struct cmd_results *checkarg(int argc, const char *name, enum expected_args type, int val) {
	struct cmd_results *error = NULL;
	switch (type) {
	case EXPECTED_MORE_THAN:
		if (argc > val) {
			return NULL;
		}
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected more than %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_AT_LEAST:
		if (argc >= val) {
			return NULL;
		}
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected at least %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_LESS_THAN:
		if (argc  < val) {
			return NULL;
		};
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected less than %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_EQUAL_TO:
		if (argc == val) {
			return NULL;
		};
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected %d arguments, got %d)", name, val, argc);
		break;
	}
	return error;
}

static struct cmd_results *cmd_assign(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "assign", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	char *criteria = *argv++;

	if (strncmp(*argv, "→", strlen("→")) == 0) {
		if (argc < 3) {
			return cmd_results_new(CMD_INVALID, "assign", "Missing workspace");
		}
		argv++;
	}

	char *movecmd = "move container to workspace ";
	int arglen = strlen(movecmd) + strlen(*argv) + 1;
	char *cmdlist = calloc(1, arglen);

	snprintf(cmdlist, arglen, "%s%s", movecmd, *argv);

	struct criteria *crit = malloc(sizeof(struct criteria));
	crit->crit_raw = strdup(criteria);
	crit->cmdlist = cmdlist;
	crit->tokens = create_list();
	char *err_str = extract_crit_tokens(crit->tokens, crit->crit_raw);

	if (err_str) {
		error = cmd_results_new(CMD_INVALID, "assign", err_str);
		free(err_str);
		free_criteria(crit);
	} else if (crit->tokens->length == 0) {
		error = cmd_results_new(CMD_INVALID, "assign", "Found no name/value pairs in criteria");
		free_criteria(crit);
	} else if (list_seq_find(config->criteria, criteria_cmp, crit) != -1) {
		sway_log(L_DEBUG, "assign: Duplicate, skipping.");
		free_criteria(crit);
	} else {
		sway_log(L_DEBUG, "assign: '%s' -> '%s' added", crit->crit_raw, crit->cmdlist);
		list_add(config->criteria, crit);
	}
	return error ? error : cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

int binding_order = 0;

static struct cmd_results *cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->release = false;
	binding->bindcode = false;

	// Handle --release
	if (strcmp("--release", argv[0]) == 0) {
		if (argc >= 3) {
			binding->release = true;
			argv++;
			argc--;
		} else {
			free_sway_binding(binding);
			return cmd_results_new(CMD_FAILURE, "bindsym",
				"Invalid bindsym command "
				"(expected more than 2 arguments, got %d)", argc);
		}
	}

	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		if ((mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			binding->modifiers |= mod;
			continue;
		}
		// Check for xkb key
		xkb_keysym_t sym = xkb_keysym_from_name(split->items[i], XKB_KEYSYM_CASE_INSENSITIVE);
		if (!sym) {
			error = cmd_results_new(CMD_INVALID, "bindsym", "Unknown key '%s'", (char *)split->items[i]);
			free_sway_binding(binding);
			list_free(split);
			return error;
		}
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		*key = sym;
		list_add(binding->keys, key);
	}
	free_flat_list(split);

	struct sway_mode *mode = config->current_mode;
	int i = list_seq_find(mode->bindings, sway_binding_cmp_keys, binding);
	if (i > -1) {
		sway_log(L_DEBUG, "bindsym - '%s' already exists, overwriting", argv[0]);
		struct sway_binding *dup = mode->bindings->items[i];
		free_sway_binding(dup);
		list_del(mode->bindings, i);
	}
	binding->order = binding_order++;
	list_add(mode->bindings, binding);
	list_qsort(mode->bindings, sway_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_bindcode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindcode", EXPECTED_MORE_THAN, 1))) {
		return error;
	}

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->release = false;
	binding->bindcode = true;

	// Handle --release
	if (strcmp("--release", argv[0]) == 0) {
		if (argc >= 3) {
			binding->release = true;
			argv++;
			argc--;
		} else {
			free_sway_binding(binding);
			return cmd_results_new(CMD_FAILURE, "bindcode",
				"Invalid bindcode command "
				"(expected more than 2 arguments, got %d)", argc);
		}
	}

	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		uint32_t mod;
		if ((mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			binding->modifiers |= mod;
			continue;
		}
		// parse keycode
		xkb_keycode_t keycode = (int)strtol(split->items[i], NULL, 10);
		if (!xkb_keycode_is_legal_ext(keycode)) {
			error = cmd_results_new(CMD_INVALID, "bindcode", "Invalid keycode '%s'", (char *)split->items[i]);
			free_sway_binding(binding);
			list_free(split);
			return error;
		}
		xkb_keycode_t *key = malloc(sizeof(xkb_keycode_t));
		*key = keycode - 8;
		list_add(binding->keys, key);
	}
	free_flat_list(split);

	struct sway_mode *mode = config->current_mode;
	int i = list_seq_find(mode->bindings, sway_binding_cmp_keys, binding);
	if (i > -1) {
		struct sway_binding *dup = mode->bindings->items[i];
		if (dup->bindcode) {
			sway_log(L_DEBUG, "bindcode - '%s' already exists, overwriting", argv[0]);
		} else {
			sway_log(L_DEBUG, "bindcode - '%s' already exists as bindsym, overwriting", argv[0]);
		}
		free_sway_binding(dup);
		list_del(mode->bindings, i);
	}
	binding->order = binding_order++;
	list_add(mode->bindings, binding);
	list_qsort(mode->bindings, sway_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindcode - Bound %s to command %s", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_border(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->active) {
		return cmd_results_new(CMD_FAILURE, "border", "Can only be used when sway is running.");
	}
	if ((error = checkarg(argc, "border", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (argc > 2) {
		return cmd_results_new(CMD_INVALID, "border",
			"Expected 'border <normal|pixel|none|toggle> [<n>]");
	}

	swayc_t *view = get_focused_view(&root_container);
	enum swayc_border_types border = view->border_type;
	int thickness = view->border_thickness;

	if (strcasecmp(argv[0], "none") == 0) {
		border = B_NONE;
	} else if (strcasecmp(argv[0], "normal") == 0) {
		border = B_NORMAL;
	} else if (strcasecmp(argv[0], "pixel") == 0) {
		border = B_PIXEL;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		switch (border) {
		case B_NONE:
			border = B_PIXEL;
			break;
		case B_NORMAL:
			border = B_NONE;
			break;
		case B_PIXEL:
			border = B_NORMAL;
			break;
		}
	} else {
		return cmd_results_new(CMD_INVALID, "border",
			"Expected 'border <normal|pixel|none|toggle>");
	}

	if (argc == 2 && (border == B_NORMAL || border == B_PIXEL)) {
		thickness = (int)strtol(argv[1], NULL, 10);
		if (errno == ERANGE || thickness < 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "border", "Number is out out of range.");
		}
	}

	if (view) {
		view->border_type = border;
		view->border_thickness = thickness;
		update_geometry(view);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *parse_border_color(struct border_colors *border_colors, const char *cmd_name, int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (argc != 5) {
		return cmd_results_new(CMD_INVALID, cmd_name, "Requires exactly five color values");
	}

	uint32_t colors[5];
	int i;
	for (i = 0; i < 5; i++) {
		char buffer[10];
		error = add_color(cmd_name, buffer, argv[i]);
		if (error) {
			return error;
		}
		colors[i] = strtoul(buffer+1, NULL, 16);
	}

	border_colors->border = colors[0];
	border_colors->background = colors[1];
	border_colors->text = colors[2];
	border_colors->indicator = colors[3];
	border_colors->child_border = colors[4];

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_client_focused(int argc, char **argv) {
	return parse_border_color(&config->border_colors.focused, "client.focused", argc, argv);
}

static struct cmd_results *cmd_client_focused_inactive(int argc, char **argv) {
	return parse_border_color(&config->border_colors.focused_inactive, "client.focused_inactive", argc, argv);
}

static struct cmd_results *cmd_client_unfocused(int argc, char **argv) {
	return parse_border_color(&config->border_colors.unfocused, "client.unfocused", argc, argv);
}

static struct cmd_results *cmd_client_urgent(int argc, char **argv) {
	return parse_border_color(&config->border_colors.urgent, "client.urgent", argc, argv);
}

static struct cmd_results *cmd_client_placeholder(int argc, char **argv) {
	return parse_border_color(&config->border_colors.placeholder, "client.placeholder", argc, argv);
}

static struct cmd_results *cmd_client_background(int argc, char **argv) {
	char buffer[10];
	struct cmd_results *error = NULL;
	uint32_t background;

	if (argc != 1) {
		return cmd_results_new(CMD_INVALID, "client.background", "Requires exactly one color value");
	}

	error = add_color("client.background", buffer, argv[0]);
	if (error) {
		return error;
	}

	background = strtoul(buffer+1, NULL, 16);
	config->border_colors.background = background;
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_exec_always(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->active) return cmd_results_new(CMD_DEFER, NULL, NULL);
	if ((error = checkarg(argc, "exec_always", EXPECTED_MORE_THAN, 0))) {
		return error;
	}

	char *tmp = NULL;
	if (strcmp((char*)*argv, "--no-startup-id") == 0) {
		sway_log(L_INFO, "exec switch '--no-startup-id' not supported, ignored.");
		if ((error = checkarg(argc - 1, "exec_always", EXPECTED_MORE_THAN, 0))) {
			return error;
		}

		tmp = join_args(argv + 1, argc - 1);
	} else {
		tmp = join_args(argv, argc);
	}

	// Put argument into cmd array
	char cmd[4096];
	strncpy(cmd, tmp, sizeof(cmd));
	cmd[sizeof(cmd) - 1] = 0;
	free(tmp);
	sway_log(L_DEBUG, "Executing %s", cmd);

	int fd[2];
	if (pipe(fd) != 0) {
		sway_log(L_ERROR, "Unable to create pipe for fork");
	}

	pid_t pid;
	pid_t *child = malloc(sizeof(pid_t)); // malloc'd so that Linux can avoid copying the process space
	// Fork process
	if ((pid = fork()) == 0) {
		// Fork child process again
		setsid();
		if ((*child = fork()) == 0) {
			execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
			/* Not reached */
		}
		close(fd[0]);
		ssize_t s = 0;
		while ((size_t)s < sizeof(pid_t)) {
			s += write(fd[1], ((uint8_t *)child) + s, sizeof(pid_t) - s);
		}
		close(fd[1]);
		_exit(0); // Close child process
	} else if (pid < 0) {
		free(child);
		return cmd_results_new(CMD_FAILURE, "exec_always", "Command failed (sway could not fork).");
	}
	close(fd[1]); // close write
	ssize_t s = 0;
	while ((size_t)s < sizeof(pid_t)) {
		s += read(fd[0], ((uint8_t *)child) + s, sizeof(pid_t) - s);
	}
	close(fd[0]);
	// cleanup child process
	wait(0);
	swayc_t *ws = swayc_active_workspace();
	if (*child > 0 && ws) {
		sway_log(L_DEBUG, "Child process created with pid %d for workspace %s", *child, ws->name);
		struct pid_workspace *pw = malloc(sizeof(struct pid_workspace));
		pw->pid = child;
		pw->workspace = strdup(ws->name);
		pid_workspace_add(pw);
		// TODO: keep track of this pid and open the corresponding view on the current workspace
		// blocked pending feature in wlc
	} else {
		free(child);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_debuglog(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "debuglog", EXPECTED_EQUAL_TO, 1))) {
		return error;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		if (config->reading) {
			return cmd_results_new(CMD_FAILURE, "debuglog toggle", "Can't be used in config file.");
		}
		if (toggle_debug_logging()) {
			sway_log(L_DEBUG, "Debuglog turned on.");
		}
	} else if (strcasecmp(argv[0], "on") == 0) {
		set_log_level(L_DEBUG);
		sway_log(L_DEBUG, "Debuglog turned on.");
	} else if (strcasecmp(argv[0], "off") == 0) {
		reset_log_level();
	} else {
		return cmd_results_new(CMD_FAILURE, "debuglog", "Expected 'debuglog on|off|toggle'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_exec(int argc, char **argv) {
	if (!config->active) return cmd_results_new(CMD_DEFER, "exec", NULL);
	if (config->reloading) {
		char *args = join_args(argv, argc);
		sway_log(L_DEBUG, "Ignoring 'exec %s' due to reload", args);
		free(args);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return cmd_exec_always(argc, argv);
}

static struct cmd_results *cmd_exit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "exit", "Can't be used in config file.");
	if ((error = checkarg(argc, "exit", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	// Close all views
	close_views(&root_container);
	sway_terminate(EXIT_SUCCESS);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_floating(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "floating", "Can't be used in config file.");
	if ((error = checkarg(argc, "floating", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	swayc_t *view = get_focused_container(&root_container);
	bool wants_floating;
	if (strcasecmp(argv[0], "enable") == 0) {
		wants_floating = true;
	} else if (strcasecmp(argv[0], "disable") == 0) {
		wants_floating = false;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		wants_floating = !view->is_floating;
	} else {
		return cmd_results_new(CMD_FAILURE, "floating",
			"Expected 'floating <enable|disable|toggle>");
	}

	// Prevent running floating commands on things like workspaces
	if (view->type != C_VIEW) {
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	// Change from nonfloating to floating
	if (!view->is_floating && wants_floating) {
		// Remove view from its current location
		destroy_container(remove_child(view));

		// and move it into workspace floating
		add_floating(swayc_active_workspace(), view);
		view->x = (swayc_active_workspace()->width - view->width)/2;
		view->y = (swayc_active_workspace()->height - view->height)/2;
		if (view->desired_width != -1) {
			view->width = view->desired_width;
		}
		if (view->desired_height != -1) {
			view->height = view->desired_height;
		}
		arrange_windows(swayc_active_workspace(), -1, -1);

	} else if (view->is_floating && !wants_floating) {
		// Delete the view from the floating list and unset its is_floating flag
		remove_child(view);
		view->is_floating = false;
		// Get the properly focused container, and add in the view there
		swayc_t *focused = container_under_pointer();
		// If focused is null, it's because the currently focused container is a workspace
		if (focused == NULL || focused->is_floating) {
			focused = swayc_active_workspace();
		}
		set_focused_container(focused);

		sway_log(L_DEBUG, "Non-floating focused container is %p", focused);

		// Case of focused workspace, just create as child of it
		if (focused->type == C_WORKSPACE) {
			add_child(focused, view);
		}
		// Regular case, create as sibling of current container
		else {
			add_sibling(focused, view);
		}
		// Refocus on the view once its been put back into the layout
		view->width = view->height = 0;
		arrange_windows(swayc_active_workspace(), -1, -1);
		remove_view_from_scratchpad(view);
		ipc_event_window(view, "floating");
	}
	set_focused_container(view);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_floating_maximum_size(int argc, char **argv) {
        struct cmd_results *error = NULL;
        int32_t width;
        int32_t height;
        char *ptr;

        if ((error = checkarg(argc, "floating_maximum_size", EXPECTED_EQUAL_TO, 3))) {
                return error;
        }
        width = strtol(argv[0], &ptr, 10);
        height = strtol(argv[2], &ptr, 10);

        if (width < -1) {
                sway_log(L_DEBUG, "floating_maximum_size invalid width value: '%s'", argv[0]);

        } else {
                config->floating_maximum_width = width;

        }

        if (height < -1) {
                sway_log(L_DEBUG, "floating_maximum_size invalid height value: '%s'", argv[2]);
        }
        else {
                config->floating_maximum_height = height;

        }

        sway_log(L_DEBUG, "New floating_maximum_size: '%d' x '%d'", config->floating_maximum_width,
                config->floating_maximum_height);

        return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_floating_minimum_size(int argc, char **argv) {
	struct cmd_results *error = NULL;
	int32_t width;
	int32_t height;
	char *ptr;

	if ((error = checkarg(argc, "floating_minimum_size", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}
	width = strtol(argv[0], &ptr, 10);
	height = strtol(argv[2], &ptr, 10);

	if (width <= 0) {
		sway_log(L_DEBUG, "floating_minimum_size invalid width value: '%s'", argv[0]);

	} else {
		config->floating_minimum_width = width;

	}

	if (height <= 0) {
		sway_log(L_DEBUG, "floating_minimum_size invalid height value: '%s'", argv[2]);
	}
	else {
		config->floating_minimum_height = height;

	}

	sway_log(L_DEBUG, "New floating_minimum_size: '%d' x '%d'", config->floating_minimum_width,
		config->floating_minimum_height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_floating_mod(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_modifier", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	int i;
	list_t *split = split_string(argv[0], "+");
	config->floating_mod = 0;

	// set modifier keys
	for (i = 0; i < split->length; ++i) {
		config->floating_mod |= get_modifier_mask_by_name(split->items[i]);
	}
	free_flat_list(split);
	if (!config->floating_mod) {
		error = cmd_results_new(CMD_INVALID, "floating_modifier", "Unknown keys %s", argv[0]);
		return error;
	}

	if (argc >= 2) {
		if (strcasecmp("inverse", argv[1]) == 0) {
			config->dragging_key = M_RIGHT_CLICK;
			config->resizing_key = M_LEFT_CLICK;
		} else if (strcasecmp("normal", argv[1]) == 0) {
			config->dragging_key = M_LEFT_CLICK;
			config->resizing_key = M_RIGHT_CLICK;
		} else {
			error = cmd_results_new(CMD_INVALID, "floating_modifier", "Invalid definition %s", argv[1]);
			return error;
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_floating_scroll(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_scroll", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!strcasecmp("up", argv[0])) {
		free(config->floating_scroll_up_cmd);
		if (argc < 2) {
			config->floating_scroll_up_cmd = strdup("");
		} else {
			config->floating_scroll_up_cmd = join_args(argv + 1, argc - 1);
		}
	} else if (!strcasecmp("down", argv[0])) {
		free(config->floating_scroll_down_cmd);
		if (argc < 2) {
			config->floating_scroll_down_cmd = strdup("");
		} else {
			config->floating_scroll_down_cmd = join_args(argv + 1, argc - 1);
		}
	} else if (!strcasecmp("left", argv[0])) {
		free(config->floating_scroll_left_cmd);
		if (argc < 2) {
			config->floating_scroll_left_cmd = strdup("");
		} else {
			config->floating_scroll_left_cmd = join_args(argv + 1, argc - 1);
		}
	} else if (!strcasecmp("right", argv[0])) {
		free(config->floating_scroll_right_cmd);
		if (argc < 2) {
			config->floating_scroll_right_cmd = strdup("");
		} else {
			config->floating_scroll_right_cmd = join_args(argv + 1, argc - 1);
		}
	} else {
		error = cmd_results_new(CMD_INVALID, "floating_scroll", "Unknown command: '%s'", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_focus(int argc, char **argv) {
	if (config->reading) return cmd_results_new(CMD_FAILURE, "focus", "Can't be used in config file.");
	struct cmd_results *error = NULL;
	if (argc > 0 && strcasecmp(argv[0], "output") == 0) {
		swayc_t *output = NULL;
		struct wlc_point abs_pos;
		get_absolute_center_position(get_focused_container(&root_container), &abs_pos);
		if ((error = checkarg(argc, "focus", EXPECTED_EQUAL_TO, 2))) {
			return error;
		} else if (!(output = output_by_name(argv[1], &abs_pos))) {
			return cmd_results_new(CMD_FAILURE, "focus output",
				"Can't find output with name/at direction '%s' @ (%i,%i)", argv[1], abs_pos.x, abs_pos.y);
		} else if (!workspace_switch(swayc_active_workspace_for(output))) {
			return cmd_results_new(CMD_FAILURE, "focus output",
				"Switching to workspace on output '%s' was blocked", argv[1]);
		} else if (config->mouse_warping) {
			swayc_t *focused = get_focused_view(output);
			if (focused && focused->type == C_VIEW) {
				center_pointer_on(focused);
			}
		}
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if ((error = checkarg(argc, "focus", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	static int floating_toggled_index = 0;
	static int tiled_toggled_index = 0;
	if (strcasecmp(argv[0], "left") == 0) {
		move_focus(MOVE_LEFT);
	} else if (strcasecmp(argv[0], "right") == 0) {
		move_focus(MOVE_RIGHT);
	} else if (strcasecmp(argv[0], "up") == 0) {
		move_focus(MOVE_UP);
	} else if (strcasecmp(argv[0], "down") == 0) {
		move_focus(MOVE_DOWN);
	} else if (strcasecmp(argv[0], "parent") == 0) {
		move_focus(MOVE_PARENT);
	} else if (strcasecmp(argv[0], "mode_toggle") == 0) {
		int i;
		swayc_t *workspace = swayc_active_workspace();
		swayc_t *focused = get_focused_view(workspace);
		if (focused->is_floating) {
			if (workspace->children->length > 0) {
				for (i = 0;i < workspace->floating->length; i++) {
					if (workspace->floating->items[i] == focused) {
						floating_toggled_index = i;
						break;
					}
				}
				if (workspace->children->length > tiled_toggled_index) {
					set_focused_container(get_focused_view(workspace->children->items[tiled_toggled_index]));
				} else {
					set_focused_container(get_focused_view(workspace->children->items[0]));
					tiled_toggled_index = 0;
				}
			}
		} else {
			if (workspace->floating->length > 0) {
				for (i = 0;i < workspace->children->length; i++) {
					if (workspace->children->items[i] == focused) {
						tiled_toggled_index = i;
						break;
					}
				}
				if (workspace->floating->length > floating_toggled_index) {
					swayc_t *floating = workspace->floating->items[floating_toggled_index];
					set_focused_container(get_focused_view(floating));
				} else {
					swayc_t *floating = workspace->floating->items[workspace->floating->length - 1];
					set_focused_container(get_focused_view(floating));
					tiled_toggled_index = workspace->floating->length - 1;
				}
			}
		}
	} else {
		return cmd_results_new(CMD_INVALID, "focus",
				"Expected 'focus <direction|parent|mode_toggle>' or 'focus output <direction|name>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_focus_follows_mouse(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focus_follows_mouse", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->focus_follows_mouse = !strcasecmp(argv[0], "yes");
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_seamless_mouse(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "seamless_mouse", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->seamless_mouse = (strcasecmp(argv[0], "on") == 0 || strcasecmp(argv[0], "yes") == 0);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static void hide_view_in_scratchpad(swayc_t *sp_view) {
	if (sp_view == NULL) {
		return;
	}

	wlc_view_set_mask(sp_view->handle, 0);
	sp_view->visible = false;
	swayc_t *ws = sp_view->parent;
	remove_child(sp_view);
	if (swayc_active_workspace() != ws && ws->floating->length == 0 && ws->children->length == 0) {
		destroy_workspace(ws);
	} else {
		arrange_windows(ws, -1, -1);
	}
	set_focused_container(container_under_pointer());
}

static struct cmd_results *cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	const char *mode_name = argv[0];
	bool mode_make = (argc == 2 && strcmp(argv[1], "{") == 0);
	if (mode_make) {
		if (!config->reading)
			return cmd_results_new(CMD_FAILURE, "mode", "Can only be used in config file.");
	}
	struct sway_mode *mode = NULL;
	// Find mode
	int i, len = config->modes->length;
	for (i = 0; i < len; ++i) {
		struct sway_mode *find = config->modes->items[i];
		if (strcasecmp(find->name, mode_name) == 0) {
			mode = find;
			break;
		}
	}
	// Create mode if it doesn't exist
	if (!mode && mode_make) {
		mode = malloc(sizeof*mode);
		mode->name = strdup(mode_name);
		mode->bindings = create_list();
		list_add(config->modes, mode);
	}
	if (!mode) {
		error = cmd_results_new(CMD_INVALID, "mode", "Unknown mode `%s'", mode_name);
		return error;
	}
	if ((config->reading && mode_make) || (!config->reading && !mode_make)) {
		sway_log(L_DEBUG, "Switching to mode `%s'",mode->name);
	}
	// Set current mode
	config->current_mode = mode;
	if (!mode_make) {
		// trigger IPC mode event
		ipc_event_mode(config->current_mode->name);
	}
	return cmd_results_new(mode_make ? CMD_BLOCK_MODE : CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_mouse_warping(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mouse_warping", EXPECTED_EQUAL_TO, 1))) {
		return error;
	} else if (strcasecmp(argv[0], "output") == 0) {
		config->mouse_warping = true;
	} else if (strcasecmp(argv[0], "none") == 0) {
		config->mouse_warping = false;
	} else {
		return cmd_results_new(CMD_FAILURE, "mouse_warping", "Expected 'mouse_warping output|none'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_move(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "move", "Can't be used in config file.");
	if ((error = checkarg(argc, "move", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char* expected_syntax = "Expected 'move <left|right|up|down>' or "
		"'move <container|window> to workspace <name>' or "
		"'move <container|window|workspace> to output <name|direction>' or "
                "'move position mouse'";
	swayc_t *view = get_focused_container(&root_container);

	if (strcasecmp(argv[0], "left") == 0) {
		move_container(view, MOVE_LEFT);
	} else if (strcasecmp(argv[0], "right") == 0) {
		move_container(view, MOVE_RIGHT);
	} else if (strcasecmp(argv[0], "up") == 0) {
		move_container(view, MOVE_UP);
	} else if (strcasecmp(argv[0], "down") == 0) {
		move_container(view, MOVE_DOWN);
	} else if (strcasecmp(argv[0], "container") == 0 || strcasecmp(argv[0], "window") == 0) {
		// "move container ...
		if ((error = checkarg(argc, "move container/window", EXPECTED_AT_LEAST, 4))) {
			return error;
		} else if ( strcasecmp(argv[1], "to") == 0 && strcasecmp(argv[2], "workspace") == 0) {
			// move container to workspace x
			if (view->type != C_CONTAINER && view->type != C_VIEW) {
				return cmd_results_new(CMD_FAILURE, "move", "Can only move containers and views.");
			}

			const char *ws_name = argv[3];
			swayc_t *ws;
			if (argc == 5 && strcasecmp(ws_name, "number") == 0) {
				// move "container to workspace number x"
				ws_name = argv[4];
				ws = workspace_by_number(ws_name);
			} else {
				ws = workspace_by_name(ws_name);
			}

			if (ws == NULL) {
				ws = workspace_create(ws_name);
			}
			move_container_to(view, get_focused_container(ws));
		} else if (strcasecmp(argv[1], "to") == 0 && strcasecmp(argv[2], "output") == 0) {
			// move container to output x
			swayc_t *output = NULL;
			struct wlc_point abs_pos;
			get_absolute_center_position(view, &abs_pos);
			if (view->type != C_CONTAINER && view->type != C_VIEW) {
				return cmd_results_new(CMD_FAILURE, "move", "Can only move containers and views.");
			} else if (!(output = output_by_name(argv[3], &abs_pos))) {
				return cmd_results_new(CMD_FAILURE, "move",
					"Can't find output with name/direction '%s' @ (%i,%i)", argv[3], abs_pos.x, abs_pos.y);
			} else {
				swayc_t *container = get_focused_container(output);
				if (container->is_floating) {
					move_container_to(view, container->parent);
				} else {
					move_container_to(view, container);
				}
			}
		} else {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		// move workspace (to output x)
		swayc_t *output = NULL;
		struct wlc_point abs_pos;
		get_absolute_center_position(view, &abs_pos);
		if ((error = checkarg(argc, "move workspace", EXPECTED_EQUAL_TO, 4))) {
			return error;
		} else if (strcasecmp(argv[1], "to") != 0 || strcasecmp(argv[2], "output") != 0) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		} else if (!(output = output_by_name(argv[3], &abs_pos))) {
			return cmd_results_new(CMD_FAILURE, "move workspace",
				"Can't find output with name/direction '%s' @ (%i,%i)", argv[3], abs_pos.x, abs_pos.y);
		}
		if (view->type == C_WORKSPACE) {
			// This probably means we're moving an empty workspace, but
			// that's fine.
			move_workspace_to(view, output);
		} else {
			swayc_t *workspace = swayc_parent_by_type(view, C_WORKSPACE);
			move_workspace_to(workspace, output);
		}
	} else if (strcasecmp(argv[0], "scratchpad") == 0) {
		// move scratchpad ...
		if (view->type != C_CONTAINER && view->type != C_VIEW) {
			return cmd_results_new(CMD_FAILURE, "move scratchpad", "Can only move containers and views.");
		}
		swayc_t *view = get_focused_container(&root_container);
		int i;
		for (i = 0; i < scratchpad->length; i++) {
			if (scratchpad->items[i] == view) {
				hide_view_in_scratchpad(view);
				sp_view = NULL;
				return cmd_results_new(CMD_SUCCESS, NULL, NULL);
			}
		}
		list_add(scratchpad, view);
		if (!view->is_floating) {
			destroy_container(remove_child(view));
		} else {
			remove_child(view);
		}
		wlc_view_set_mask(view->handle, 0);
		arrange_windows(swayc_active_workspace(), -1, -1);
		swayc_t *focused = container_under_pointer();
		if (focused == NULL) {
			focused = swayc_active_workspace();
		}
		set_focused_container(focused);
	} else if (strcasecmp(argv[0], "position") == 0) {
		if ((error = checkarg(argc, "move workspace", EXPECTED_EQUAL_TO, 2))) {
			return error;
		}
		if (strcasecmp(argv[1], "mouse")) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}

		if (view->is_floating) {
			swayc_t *output = swayc_parent_by_type(view, C_OUTPUT);
			struct wlc_geometry g;
			wlc_view_get_visible_geometry(view->handle, &g);
			const struct wlc_size *size = wlc_output_get_resolution(output->handle);

			struct wlc_point origin;
			wlc_pointer_get_position(&origin);

			int32_t x = origin.x - g.size.w / 2;
			int32_t y = origin.y - g.size.h / 2;

			uint32_t w = size->w - g.size.w;
			uint32_t h = size->h - g.size.h;

			view->x = g.origin.x = MIN((int32_t)w, MAX(x, 0));
			view->y = g.origin.y = MIN((int32_t)h, MAX(y, 0));

			wlc_view_set_geometry(view->handle, 0, &g);
		}
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_new_float(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "new_float", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (argc > 2) {
		return cmd_results_new(CMD_INVALID, "new_float",
			"Expected 'new_float <normal|none|pixel> [<n>]");
	}

	enum swayc_border_types border = config->floating_border;
	int thickness = config->floating_border_thickness;

	if (strcasecmp(argv[0], "none") == 0) {
		border = B_NONE;
	} else if (strcasecmp(argv[0], "normal") == 0) {
		border = B_NORMAL;
	} else if (strcasecmp(argv[0], "pixel") == 0) {
		border = B_PIXEL;
	} else {
		return cmd_results_new(CMD_INVALID, "new_float",
			"Expected 'border <normal|none|pixel>");
	}

	if (argc == 2 && (border == B_NORMAL || border == B_PIXEL)) {
		thickness = (int)strtol(argv[1], NULL, 10);
		if (errno == ERANGE || thickness < 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "new_float", "Number is out out of range.");
		}
	}

	config->floating_border = border;
	config->floating_border_thickness = thickness;

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_new_window(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "new_window", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (argc > 2) {
		return cmd_results_new(CMD_INVALID, "new_window",
			"Expected 'new_window <normal|none|pixel> [<n>]");
	}

	enum swayc_border_types border = config->border;
	int thickness = config->border_thickness;

	if (strcasecmp(argv[0], "none") == 0) {
		border = B_NONE;
	} else if (strcasecmp(argv[0], "normal") == 0) {
		border = B_NORMAL;
	} else if (strcasecmp(argv[0], "pixel") == 0) {
		border = B_PIXEL;
	} else {
		return cmd_results_new(CMD_INVALID, "new_window",
			"Expected 'border <normal|none|pixel>");
	}

	if (argc == 2 && (border == B_NORMAL || border == B_PIXEL)) {
		thickness = (int)strtol(argv[1], NULL, 10);
		if (errno == ERANGE || thickness < 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "new_window", "Number is out out of range.");
		}
	}

	config->border = border;
	config->border_thickness = thickness;

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_orientation(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->reading) return cmd_results_new(CMD_FAILURE, "orientation", "Can only be used in config file.");
	if ((error = checkarg(argc, "orientation", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "horizontal") == 0) {
		config->default_orientation = L_HORIZ;
	} else if (strcasecmp(argv[0], "vertical") == 0) {
		config->default_orientation = L_VERT;
	} else if (strcasecmp(argv[0], "auto") == 0) {
		// Do nothing
	} else {
		return cmd_results_new(CMD_INVALID, "orientation", "Expected 'orientation <horizontal|vertical|auto>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static void input_cmd_apply(struct input_config *input) {
	int i;
	i = list_seq_find(config->input_configs, input_identifier_cmp, input->identifier);
	if (i >= 0) {
		// merge existing config
		struct input_config *ic = config->input_configs->items[i];
		merge_input_config(ic, input);
		free_input_config(input);
		input = ic;
	} else {
		list_add(config->input_configs, input);
	}

	current_input_config = input;

	if (input->identifier) {
		// Try to find the input device and apply configuration now. If
		// this is during startup then there will be no container and config
		// will be applied during normal "new input" event from wlc.
		struct libinput_device *device = NULL;
		for (int i = 0; i < input_devices->length; ++i) {
			device = input_devices->items[i];
			char* dev_identifier = libinput_dev_unique_id(device);
			int match = dev_identifier && strcmp(dev_identifier, input->identifier) == 0;
			free(dev_identifier);
			if (match) {
				apply_input_config(input, device);
				break;
			}
		}
	}
}

static struct cmd_results *input_cmd_accel_profile(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "accel_profile", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "accel_profile", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "adaptive") == 0) {
		new_config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	} else if (strcasecmp(argv[0], "flat") == 0) {
		new_config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	} else {
		return cmd_results_new(CMD_INVALID, "accel_profile",
				"Expected 'accel_profile <adaptive|flat>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_click_method(int argc, char **argv) {
	sway_log(L_DEBUG, "click_method for device:  %d %s", current_input_config==NULL, current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "click_method", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "click_method", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "none") == 0) {
		new_config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
	} else if (strcasecmp(argv[0], "button_areas") == 0) {
		new_config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	} else if (strcasecmp(argv[0], "clickfinger") == 0) {
		new_config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
	} else {
		return cmd_results_new(CMD_INVALID, "click_method", "Expected 'click_method <none|button_areas|clickfinger'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_drag_lock(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "drag_lock", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "drag_lock", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID, "drag_lock", "Expected 'drag_lock <enabled|disabled>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_dwt(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "dwt", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "dwt", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->dwt = LIBINPUT_CONFIG_DWT_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->dwt = LIBINPUT_CONFIG_DWT_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID, "dwt", "Expected 'dwt <enabled|disabled>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_events(int argc, char **argv) {
	sway_log(L_DEBUG, "events for device: %s", current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "events", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "events", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->send_events = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	} else if (strcasecmp(argv[0], "disabled_on_external_mouse") == 0) {
		new_config->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else {
		return cmd_results_new(CMD_INVALID, "events", "Expected 'events <enabled|disabled|disabled_on_external_mouse>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_middle_emulation(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "middle_emulation", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "middle_emulation", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID, "middle_emulation", "Expected 'middle_emulation <enabled|disabled>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_natural_scroll(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "natural_scroll", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "natural_scoll", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->natural_scroll = 1;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->natural_scroll = 0;
	} else {
		return cmd_results_new(CMD_INVALID, "natural_scroll", "Expected 'natural_scroll <enabled|disabled>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_pointer_accel(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pointer_accel", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "pointer_accel", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	float pointer_accel = atof(argv[0]);
	if (pointer_accel < -1 || pointer_accel > 1) {
		return cmd_results_new(CMD_INVALID, "pointer_accel", "Input out of range [-1, 1]");
	}
	new_config->pointer_accel = pointer_accel;

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_scroll_method(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_method", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "scroll_method", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "none") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
	} else if (strcasecmp(argv[0], "two_finger") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	} else if (strcasecmp(argv[0], "edge") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
	} else if (strcasecmp(argv[0], "on_button_down") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
	} else {
		return cmd_results_new(CMD_INVALID, "scroll_method", "Expected 'scroll_method <none|two_finger|edge|on_button_down>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *input_cmd_tap(int argc, char **argv) {
	sway_log(L_DEBUG, "tap for device: %s", current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tap", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "tap", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->tap = LIBINPUT_CONFIG_TAP_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->tap = LIBINPUT_CONFIG_TAP_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID, "tap", "Expected 'tap <enabled|disabled>'");
	}

	sway_log(L_DEBUG, "apply-tap for device: %s", current_input_config->identifier);
	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_include(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "include", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!load_include_configs(argv[0], config)) {
		return cmd_results_new(CMD_INVALID, "include", "Failed to include sub configuration file: %s", argv[0]);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_input(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "input", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[1]) == 0) {
		current_input_config = new_input_config(argv[0]);
		sway_log(L_DEBUG, "entering input block: %s", current_input_config->identifier);
		return cmd_results_new(CMD_BLOCK_INPUT, NULL, NULL);
	}

	if (argc > 2) {
		int argc_new = argc-2;
		char **argv_new = argv+2;

		struct cmd_results *res;
		current_input_config = new_input_config(argv[0]);
		if (strcasecmp("accel_profile", argv[1]) == 0) {
			res = input_cmd_accel_profile(argc_new, argv_new);
		} else if (strcasecmp("click_method", argv[1]) == 0) {
			res = input_cmd_click_method(argc_new, argv_new);
		} else if (strcasecmp("drag_lock", argv[1]) == 0) {
			res = input_cmd_drag_lock(argc_new, argv_new);
		} else if (strcasecmp("dwt", argv[1]) == 0) {
			res = input_cmd_dwt(argc_new, argv_new);
		} else if (strcasecmp("events", argv[1]) == 0) {
			res = input_cmd_events(argc_new, argv_new);
		} else if (strcasecmp("middle_emulation", argv[1]) == 0) {
			res = input_cmd_middle_emulation(argc_new, argv_new);
		} else if (strcasecmp("natural_scroll", argv[1]) == 0) {
			res = input_cmd_natural_scroll(argc_new, argv_new);
		} else if (strcasecmp("pointer_accel", argv[1]) == 0) {
			res = input_cmd_pointer_accel(argc_new, argv_new);
		} else if (strcasecmp("scroll_method", argv[1]) == 0) {
			res = input_cmd_scroll_method(argc_new, argv_new);
		} else if (strcasecmp("tap", argv[1]) == 0) {
			res = input_cmd_tap(argc_new, argv_new);
		} else {
			res = cmd_results_new(CMD_INVALID, "input <device>", "Unknown command %s", argv[1]);
		}
		current_input_config = NULL;
		return res;
	}

	return cmd_results_new(CMD_BLOCK_INPUT, NULL, NULL);
}

static struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char *name = argv[0];

	struct output_config *output = calloc(1, sizeof(struct output_config));
	output->x = output->y = output->width = output->height = -1;
	output->name = strdup(name);
	output->enabled = -1;

	// TODO: atoi doesn't handle invalid numbers

	int i;
	for (i = 1; i < argc; ++i) {
		const char *command = argv[i];

		if (strcasecmp(command, "disable") == 0) {
			output->enabled = 0;
		} else if (strcasecmp(command, "resolution") == 0 || strcasecmp(command, "res") == 0) {
			if (++i >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing resolution argument.");
			}
			char *res = argv[i];
			char *x = strchr(res, 'x');
			int width = -1, height = -1;
			if (x != NULL) {
				// Format is 1234x4321
				*x = '\0';
				width = atoi(res);
				height = atoi(x + 1);
				*x = 'x';
			} else {
				// Format is 1234 4321
				width = atoi(res);
				if (++i >= argc) {
					return cmd_results_new(CMD_INVALID, "output", "Missing resolution argument (height).");
				}
				res = argv[i];
				height = atoi(res);
			}
			output->width = width;
			output->height = height;
		} else if (strcasecmp(command, "position") == 0 || strcasecmp(command, "pos") == 0) {
			if (++i >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing position argument.");
			}
			char *res = argv[i];
			char *c = strchr(res, ',');
			int x = -1, y = -1;
			if (c != NULL) {
				// Format is 1234,4321
				*c = '\0';
				x = atoi(res);
				y = atoi(c + 1);
				*c = ',';
			} else {
				// Format is 1234 4321
				x = atoi(res);
				if (++i >= argc) {
					return cmd_results_new(CMD_INVALID, "output", "Missing position argument (y).");
				}
				res = argv[i];
				y = atoi(res);
			}
			output->x = x;
			output->y = y;
		} else if (strcasecmp(command, "background") == 0 || strcasecmp(command, "bg") == 0) {
			wordexp_t p;
			if (++i >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing background file.");
			}
			if (i + 1 >= argc) {
				return cmd_results_new(CMD_INVALID, "output", "Missing background scaling mode.");
			}
			char *src = join_args(argv + i, argc - i - 1);
			char *mode = argv[argc - 1];
			if (wordexp(src, &p, 0) != 0 || p.we_wordv[0] == NULL) {
				return cmd_results_new(CMD_INVALID, "output", "Invalid syntax (%s)", src);
			}
			free(src);
			src = p.we_wordv[0];
			if (config->reading && *src != '/') {
				char *conf = strdup(config->current_config);
				char *conf_path = dirname(conf);
				src = malloc(strlen(conf_path) + strlen(src) + 2);
				sprintf(src, "%s/%s", conf_path, p.we_wordv[0]);
				free(conf);
			}
			if (access(src, F_OK) == -1) {
				return cmd_results_new(CMD_INVALID, "output", "Background file unreadable (%s)", src);
			}
			for (char *m = mode; *m; ++m) *m = tolower(*m);
			// Check mode
			bool valid = false;
			size_t j;
			for (j = 0; j < sizeof(bg_options) / sizeof(char *); ++j) {
				if (strcasecmp(mode, bg_options[j]) == 0) {
					valid = true;
					break;
				}
			}
			if (!valid) {
				return cmd_results_new(CMD_INVALID, "output", "Invalid background scaling mode.");
			}
			output->background = strdup(src);
			output->background_option = strdup(mode);
			if (src != p.we_wordv[0]) {
				free(src);
			}
			wordfree(&p);
		}
	}

	i = list_seq_find(config->output_configs, output_name_cmp, name);
	if (i >= 0) {
		// merge existing config
		struct output_config *oc = config->output_configs->items[i];
		merge_output_config(oc, output);
		free_output_config(output);
		output = oc;
	} else {
		list_add(config->output_configs, output);
	}

	sway_log(L_DEBUG, "Config stored for output %s (enabled:%d) (%d x %d @ %d, %d) (bg %s %s)",
			output->name, output->enabled, output->width,
			output->height, output->x, output->y, output->background,
			output->background_option);

	if (output->name) {
		// Try to find the output container and apply configuration now. If
		// this is during startup then there will be no container and config
		// will be applied during normal "new output" event from wlc.
		swayc_t *cont = NULL;
		for (int i = 0; i < root_container.children->length; ++i) {
			cont = root_container.children->items[i];
			if (cont->name && ((strcmp(cont->name, output->name) == 0) || (strcmp(output->name, "*") == 0))) {
				apply_output_config(output, cont);

				if (strcmp(output->name, "*") != 0) {
					// stop looking if the output config isn't applicable to all outputs
					break;
				}
			}
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_gaps(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char* expected_syntax =
		"Expected 'gaps edge_gaps <on|off|toggle>' or "
		"'gaps <inner|outer> <current|all|workspace> <set|plus|minus n>'";
	const char *amount_str = argv[0];
	// gaps amount
	if (argc >= 1 && isdigit(*amount_str)) {
		int amount = (int)strtol(amount_str, NULL, 10);
		if (errno == ERANGE) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "gaps", "Number is out out of range.");
		}
		config->gaps_inner = config->gaps_outer = amount;
		arrange_windows(&root_container, -1, -1);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	// gaps inner|outer n
	else if (argc >= 2 && isdigit((amount_str = argv[1])[0])) {
		int amount = (int)strtol(amount_str, NULL, 10);
		if (errno == ERANGE) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "gaps", "Number is out out of range.");
		}
		const char *target_str = argv[0];
		if (strcasecmp(target_str, "inner") == 0) {
			config->gaps_inner = amount;
		} else if (strcasecmp(target_str, "outer") == 0) {
			config->gaps_outer = amount;
		}
		arrange_windows(&root_container, -1, -1);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (argc == 2 && strcasecmp(argv[0], "edge_gaps") == 0) {
		// gaps edge_gaps <on|off|toggle>
		if (strcasecmp(argv[1], "toggle") == 0) {
			if (config->reading) {
				return cmd_results_new(CMD_FAILURE, "gaps edge_gaps toggle",
					"Can't be used in config file.");
			}
			config->edge_gaps = !config->edge_gaps;
		} else {
			config->edge_gaps =
				(strcasecmp(argv[1], "yes") == 0 || strcasecmp(argv[1], "on") == 0);
		}
		arrange_windows(&root_container, -1, -1);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	// gaps inner|outer current|all set|plus|minus n
	if (argc < 4 || config->reading) {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}
	// gaps inner|outer ...
	const char *inout_str = argv[0];
	enum {INNER, OUTER} inout;
	if (strcasecmp(inout_str, "inner") == 0) {
		inout = INNER;
	} else if (strcasecmp(inout_str, "outer") == 0) {
		inout = OUTER;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}

	// gaps ... current|all ...
	const char *target_str = argv[1];
	enum {CURRENT, WORKSPACE, ALL} target;
	if (strcasecmp(target_str, "current") == 0) {
		target = CURRENT;
	} else if (strcasecmp(target_str, "all") == 0) {
		target = ALL;
	} else if (strcasecmp(target_str, "workspace") == 0) {
		if (inout == OUTER) {
			target = CURRENT;
		} else {
			// Set gap for views in workspace
			target = WORKSPACE;
		}
	} else {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}

	// gaps ... n
	amount_str = argv[3];
	int amount = (int)strtol(amount_str, NULL, 10);
	if (errno == ERANGE) {
		errno = 0;
		return cmd_results_new(CMD_INVALID, "gaps", "Number is out out of range.");
	}

	// gaps ... set|plus|minus ...
	const char *method_str = argv[2];
	enum {SET, ADD} method;
	if (strcasecmp(method_str, "set") == 0) {
		method = SET;
	} else if (strcasecmp(method_str, "plus") == 0) {
		method = ADD;
	} else if (strcasecmp(method_str, "minus") == 0) {
		method = ADD;
		amount *= -1;
	} else {
		return cmd_results_new(CMD_INVALID, "gaps", expected_syntax);
	}

	if (target == CURRENT) {
		swayc_t *cont;
		if (inout == OUTER) {
			if ((cont = swayc_active_workspace()) == NULL) {
				return cmd_results_new(CMD_FAILURE, "gaps", "There's no active workspace.");
			}
		} else {
			if ((cont = get_focused_view(&root_container))->type != C_VIEW) {
				return cmd_results_new(CMD_FAILURE, "gaps", "Currently focused item is not a view.");
			}
		}
		cont->gaps = swayc_gap(cont);
		if (method == SET) {
			cont->gaps = amount;
		} else if ((cont->gaps += amount) < 0) {
			cont->gaps = 0;
		}
		arrange_windows(cont->parent, -1, -1);
	} else if (inout == OUTER) {
		//resize all workspace.
		int i,j;
		for (i = 0; i < root_container.children->length; ++i) {
			swayc_t *op = root_container.children->items[i];
			for (j = 0; j < op->children->length; ++j) {
				swayc_t *ws = op->children->items[j];
				if (method == SET) {
					ws->gaps = amount;
				} else if ((ws->gaps += amount) < 0) {
					ws->gaps = 0;
				}
			}
		}
		arrange_windows(&root_container, -1, -1);
	} else {
		// Resize gaps for all views in workspace
		swayc_t *top;
		if (target == WORKSPACE) {
			if ((top = swayc_active_workspace()) == NULL) {
				return cmd_results_new(CMD_FAILURE, "gaps", "There's currently no active workspace.");
			}
		} else {
			top = &root_container;
		}
		int top_gap = top->gaps;
		container_map(top, method == SET ? set_gaps : add_gaps, &amount);
		top->gaps = top_gap;
		arrange_windows(top, -1, -1);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_smart_gaps(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "smart_gaps", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "on") == 0) {
		config->smart_gaps = true;
	} else if (strcasecmp(argv[0], "off") == 0) {
		config->smart_gaps = false;
	} else {
		return cmd_results_new(CMD_INVALID, "smart_gaps", "Expected 'smart_gaps <on|off>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_hide_edge_borders(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_edge_borders", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "none") == 0) {
		config->hide_edge_borders = E_NONE;
	} else if (strcasecmp(argv[0], "vertical") == 0) {
		config->hide_edge_borders = E_VERTICAL;
	} else if (strcasecmp(argv[0], "horizontal") == 0) {
		config->hide_edge_borders = E_HORIZONTAL;
	} else if (strcasecmp(argv[0], "both") == 0) {
		config->hide_edge_borders = E_BOTH;
	} else {
		return cmd_results_new(CMD_INVALID, "hide_edge_borders",
				"Expected 'hide_edge_borders <none|vertical|horizontal|both>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}


static struct cmd_results *cmd_kill(int argc, char **argv) {
	if (config->reading) return cmd_results_new(CMD_FAILURE, "kill", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "kill", "Can only be used when sway is running.");

	swayc_t *view = get_focused_container(&root_container);
	wlc_view_close(view->handle);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "layout", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "layout", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "layout", EXPECTED_MORE_THAN, 0))) {
		return error;
	}
	swayc_t *parent = get_focused_container(&root_container);
	if (parent->is_floating) {
		return cmd_results_new(CMD_FAILURE, "layout", "Unable to change layout of floating windows");
	}

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	enum swayc_layouts old_layout = parent->layout;

	if (strcasecmp(argv[0], "default") == 0) {
		parent->layout = parent->prev_layout;
		if (parent->layout == L_NONE) {
			swayc_t *output = swayc_parent_by_type(parent, C_OUTPUT);
			parent->layout = default_layout(output);
		}
	} else {
		if (parent->layout != L_TABBED && parent->layout != L_STACKED) {
			parent->prev_layout = parent->layout;
		}

		if (strcasecmp(argv[0], "tabbed") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_TABBED);
			}

			parent->layout = L_TABBED;
		} else if (strcasecmp(argv[0], "stacking") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)) {
				parent = new_container(parent, L_STACKED);
			}

			parent->layout = L_STACKED;
		} else if (strcasecmp(argv[0], "splith") == 0) {
			parent->layout = L_HORIZ;
		} else if (strcasecmp(argv[0], "splitv") == 0) {
			parent->layout = L_VERT;
		} else if (strcasecmp(argv[0], "toggle") == 0 && argc == 2 && strcasecmp(argv[1], "split") == 0) {
			if (parent->layout == L_VERT) {
				parent->layout = L_HORIZ;
			} else {
				parent->layout = L_VERT;
			}
		}
	}

	update_layout_geometry(parent, old_layout);

	arrange_windows(parent, parent->width, parent->height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_reload(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "reload", "Can't be used in config file.");
	if ((error = checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	if (!load_main_config(config->current_config, true)) {
		return cmd_results_new(CMD_FAILURE, "reload", "Error(s) reloading config.");
	}

	load_swaybars();

	arrange_windows(&root_container, -1, -1);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_resize(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "resize", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "resize", "Can only be used when sway is running.");

	if (strcasecmp(argv[0], "set") == 0) {
		return cmd_resize_set(argc - 1, &argv[1]);
	}

	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	int dim_arg = argc - 1;

	enum resize_dim_types dim_type = RESIZE_DIM_DEFAULT;
	if (strcasecmp(argv[dim_arg], "ppt") == 0) {
		dim_type = RESIZE_DIM_PPT;
		dim_arg--;
	} else if (strcasecmp(argv[dim_arg], "px") == 0) {
		dim_type = RESIZE_DIM_PX;
		dim_arg--;
	}

	int amount = (int)strtol(argv[dim_arg], NULL, 10);
	if (errno == ERANGE || amount == 0) {
		errno = 0;
		amount = 10; // this is the default resize dimension used by i3 for both px and ppt
		sway_log(L_DEBUG, "Tried to get resize dimension out of '%s' but failed; setting dimension to default %d",
			argv[dim_arg], amount);
	}

	bool use_width = false;
	if (strcasecmp(argv[1], "width") == 0) {
		use_width = true;
	} else if (strcasecmp(argv[1], "height") != 0) {
		return cmd_results_new(CMD_INVALID, "resize",
			"Expected 'resize <shrink|grow> <width|height> [<amount>] [px|ppt]'");
	}

	if (strcasecmp(argv[0], "shrink") == 0) {
		amount *= -1;
	} else if (strcasecmp(argv[0], "grow") != 0) {
		return cmd_results_new(CMD_INVALID, "resize",
			"Expected 'resize <shrink|grow> <width|height> [<amount>] [px|ppt]'");
	}

	resize(amount, use_width, dim_type);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_resize_set(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "resize set", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (strcasecmp(argv[0], "width") == 0 || strcasecmp(argv[0], "height") == 0) {
		// handle `reset set width 100 px height 100 px` syntax, also allows
		// specifying only one dimension for a `resize set`
		int cmd_num = 0;
		int dim;

		while ((cmd_num + 1) < argc) {
			dim = (int)strtol(argv[cmd_num + 1], NULL, 10);
			if (errno == ERANGE || dim == 0) {
				errno = 0;
				return cmd_results_new(CMD_INVALID, "resize set",
					"Expected 'resize set <width|height> <amount> [px] [<width|height> <amount> [px]]'");
			}

			if (strcasecmp(argv[cmd_num], "width") == 0) {
				set_size(dim, true);
			} else if (strcasecmp(argv[cmd_num], "height") == 0) {
				set_size(dim, false);
			} else {
				return cmd_results_new(CMD_INVALID, "resize set",
					"Expected 'resize set <width|height> <amount> [px] [<width|height> <amount> [px]]'");
			}

			cmd_num += 2;

			if (cmd_num < argc && strcasecmp(argv[cmd_num], "px") == 0) {
				// if this was `resize set width 400 px height 300 px`, disregard the `px` arg
				cmd_num++;
			}
		}
	} else {
		// handle `reset set 100 px 100 px` syntax
		int width = (int)strtol(argv[0], NULL, 10);
		if (errno == ERANGE || width == 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "resize set",
				"Expected 'resize set <width> [px] <height> [px]'");
		}

		int height_arg = 1;
		if (strcasecmp(argv[1], "px") == 0) {
			height_arg = 2;
		}

		int height = (int)strtol(argv[height_arg], NULL, 10);
		if (errno == ERANGE || height == 0) {
			errno = 0;
			return cmd_results_new(CMD_INVALID, "resize set",
				"Expected 'resize set <width> [px] <height> [px]'");
		}

		set_size(width, true);
		set_size(height, false);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_bar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bar", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[0]) != 0) {
		return cmd_results_new(CMD_INVALID, "bar",
				"Expected '{' at start of bar config definition.");
	}

	if (!config->reading) {
		if (argc > 1) {
			if (strcasecmp("mode", argv[0]) == 0) {
				return bar_cmd_mode(argc-1, argv + 1);
			}

			if (strcasecmp("hidden_state", argv[0]) == 0) {
				return bar_cmd_hidden_state(argc-1, argv + 1);
			}
		}

		return cmd_results_new(CMD_FAILURE, "bar", "Can only be used in config file.");
	}

	// Create new bar with default values
	struct bar_config *bar = default_bar_config();

	// set bar id
	int i;
	for (i = 0; i < config->bars->length; ++i) {
		if (bar == config->bars->items[i]) {
			const int len = 5 + numlen(i); // "bar-" + i + \0
			bar->id = malloc(len * sizeof(char));
			snprintf(bar->id, len, "bar-%d", i);
			break;
		}
	}

	// Set current bar
	config->current_bar = bar;
	sway_log(L_DEBUG, "Configuring bar %s", bar->id);
	return cmd_results_new(CMD_BLOCK_BAR, NULL, NULL);
}

static swayc_t *fetch_view_from_scratchpad() {
	if (sp_index >= scratchpad->length) {
		sp_index = 0;
	}
	swayc_t *view = scratchpad->items[sp_index++];

	if (wlc_view_get_output(view->handle) != swayc_active_output()->handle) {
		wlc_view_set_output(view->handle, swayc_active_output()->handle);
	}
	if (!view->is_floating) {
		view->width = swayc_active_workspace()->width/2;
		view->height = swayc_active_workspace()->height/2;
		view->x = (swayc_active_workspace()->width - view->width)/2;
		view->y = (swayc_active_workspace()->height - view->height)/2;
	}
	if (swayc_active_workspace()->width < view->x + 20 || view->x + view->width < 20) {
		view->x = (swayc_active_workspace()->width - view->width)/2;
	}
	if (swayc_active_workspace()->height < view->y + 20 || view->y + view->height < 20) {
		view->y = (swayc_active_workspace()->height - view->height)/2;
	}

	add_floating(swayc_active_workspace(), view);
	wlc_view_set_mask(view->handle, VISIBLE);
	view->visible = true;
	arrange_windows(swayc_active_workspace(), -1, -1);
	set_focused_container(view);
	return view;
}

void remove_view_from_scratchpad(swayc_t *view) {
	int i;
	for (i = 0; i < scratchpad->length; i++) {
		if (scratchpad->items[i] == view) {
			if (sp_index == 0) {
				sp_index = scratchpad->length - 1;
			} else {
				sp_index--;
			}
			list_del(scratchpad, sp_index);
			sp_view = NULL;
		}
	}
}

static struct cmd_results *cmd_scratchpad(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "scratchpad", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "scratchpad", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "scratchpad", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "show") == 0 && scratchpad->length > 0) {
		if (!sp_view) {
			sp_view = fetch_view_from_scratchpad();
		} else {
			if (swayc_active_workspace() != sp_view->parent) {
				hide_view_in_scratchpad(sp_view);
				if (sp_index == 0) {
					sp_index = scratchpad->length;
				}
				sp_index--;
				sp_view = fetch_view_from_scratchpad();
			} else {
				hide_view_in_scratchpad(sp_view);
				sp_view = NULL;
			}
		}
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return cmd_results_new(CMD_FAILURE, "scratchpad", "Expected 'scratchpad show' when scratchpad is not empty.");
}

// sort in order of longest->shortest
static int compare_set_qsort(const void *_l, const void *_r) {
	struct sway_variable const *l = *(void **)_l;
	struct sway_variable const *r = *(void **)_r;
	return strlen(r->name) - strlen(l->name);
}

static struct cmd_results *cmd_set(int argc, char **argv) {
	char *tmp;
	int size;
	struct cmd_results *error = NULL;
	if (!config->reading) return cmd_results_new(CMD_FAILURE, "set", "Can only be used in config file.");
	if ((error = checkarg(argc, "set", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (argv[0][0] != '$') {
		sway_log(L_INFO, "Warning: variable '%s' doesn't start with $", argv[0]);

		size = asprintf(&tmp, "%s%s", "$", argv[0]);
		if (size == -1) {
			return cmd_results_new(CMD_FAILURE, "set", "Not possible to create variable $'%s'", argv[0]);
		}

		argv[0] = strdup(tmp);
		free(tmp);
	}

	struct sway_variable *var = NULL;
	// Find old variable if it exists
	int i;
	for (i = 0; i < config->symbols->length; ++i) {
		var = config->symbols->items[i];
		if (strcmp(var->name, argv[0]) == 0) {
			break;
		}
		var = NULL;
	}
	if (var) {
		free(var->value);
	} else {
		var = malloc(sizeof(struct sway_variable));
		var->name = strdup(argv[0]);
		list_add(config->symbols, var);
		list_qsort(config->symbols, compare_set_qsort);
	}
	var->value = join_args(argv + 1, argc - 1);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *_do_split(int argc, char **argv, int layout) {
	char *name = layout == L_VERT  ? "splitv" :
		layout == L_HORIZ ? "splith" : "split";
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, name, "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, name, "Can only be used when sway is running.");
	if ((error = checkarg(argc, name, EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	swayc_t *focused = get_focused_container(&root_container);

	// Case of floating window, don't split
	if (focused->is_floating) {
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	/* Case that focus is on an workspace with 0/1 children.change its layout */
	if (focused->type == C_WORKSPACE && focused->children->length <= 1) {
		sway_log(L_DEBUG, "changing workspace layout");
		focused->layout = layout;
	} else if (focused->type != C_WORKSPACE && focused->parent->children->length == 1) {
		/* Case of no siblings. change parent layout */
		sway_log(L_DEBUG, "changing container layout");
		focused->parent->layout = layout;
	} else {
		/* regular case where new split container is build around focused container
		 * or in case of workspace, container inherits its children */
		sway_log(L_DEBUG, "Adding new container around current focused container");
		sway_log(L_INFO, "FOCUSED SIZE: %.f %.f", focused->width, focused->height);
		swayc_t *parent = new_container(focused, layout);
		set_focused_container(focused);
		arrange_windows(parent, -1, -1);
	}

	// update container title if tabbed/stacked
	if (swayc_tabbed_stacked_ancestor(focused)) {
		update_view_border(focused);
		swayc_t *output = swayc_parent_by_type(focused, C_OUTPUT);
		// schedule render to make changes take effect right away,
		// otherwise we would have to wait for the view to render,
		// which is unpredictable.
		wlc_output_schedule_render(output->handle);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_split(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "split", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "split", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "split", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "v") == 0 || strcasecmp(argv[0], "vertical") == 0) {
		_do_split(argc - 1, argv + 1, L_VERT);
	} else if (strcasecmp(argv[0], "h") == 0 || strcasecmp(argv[0], "horizontal") == 0) {
		_do_split(argc - 1, argv + 1, L_HORIZ);
	} else if (strcasecmp(argv[0], "t") == 0 || strcasecmp(argv[0], "toggle") == 0) {
		swayc_t *focused = get_focused_container(&root_container);
		if (focused->parent->layout == L_VERT) {
			_do_split(argc - 1, argv + 1, L_HORIZ);
		} else {
			_do_split(argc - 1, argv + 1, L_VERT);
		}
	} else {
		error = cmd_results_new(CMD_FAILURE, "split",
			"Invalid split command (expected either horizontal or vertical).");
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_splitv(int argc, char **argv) {
	return _do_split(argc, argv, L_VERT);
}

static struct cmd_results *cmd_splith(int argc, char **argv) {
	return _do_split(argc, argv, L_HORIZ);
}

static struct cmd_results *cmd_splitt(int argc, char **argv) {
	swayc_t *focused = get_focused_container(&root_container);
	if (focused->parent->layout == L_VERT) {
		return _do_split(argc, argv, L_HORIZ);
	} else {
		return _do_split(argc, argv, L_VERT);
	}
}

static struct cmd_results *cmd_sticky(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "sticky", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "sticky", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "sticky", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	char *action = argv[0];
	swayc_t *cont = get_focused_view(&root_container);
	if (strcmp(action, "toggle") == 0) {
		cont->sticky = !cont->sticky;
	} else if (strcmp(action, "enable") == 0) {
		cont->sticky = true;
	} else if (strcmp(action, "disable") == 0) {
		cont->sticky = false;
	} else {
		return cmd_results_new(CMD_FAILURE, "sticky",
				"Expected 'sticky enable|disable|toggle'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_log_colors(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->reading) return cmd_results_new(CMD_FAILURE, "log_colors", "Can only be used in config file.");
	if ((error = checkarg(argc, "log_colors", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "no") == 0) {
		sway_log_colors(0);
	} else if (strcasecmp(argv[0], "yes") == 0) {
		sway_log_colors(1);
	} else {
		error = cmd_results_new(CMD_FAILURE, "log_colors",
			"Invalid log_colors command (expected `yes` or `no`, got '%s')", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_font(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "font", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *font = join_args(argv, argc);
	free(config->font);
	if (strlen(font) > 6 && strncmp("pango:", font, 6) == 0) {
		config->font = strdup(font + 6);
		free(font);
	} else {
		config->font = font;
	}

	config->font_height = get_font_text_height(config->font);

	sway_log(L_DEBUG, "Settings font %s", config->font);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}


static struct cmd_results *cmd_for_window(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "for_window", EXPECTED_AT_LEAST, 2))) {
		return error;
	}
	// add command to a criteria/command pair that is run against views when they appear.
	char *criteria = argv[0], *cmdlist = join_args(argv + 1, argc - 1);

	struct criteria *crit = malloc(sizeof(struct criteria));
	crit->crit_raw = strdup(criteria);
	crit->cmdlist = cmdlist;
	crit->tokens = create_list();
	char *err_str = extract_crit_tokens(crit->tokens, crit->crit_raw);

	if (err_str) {
		error = cmd_results_new(CMD_INVALID, "for_window", err_str);
		free(err_str);
		free_criteria(crit);
	} else if (crit->tokens->length == 0) {
		error = cmd_results_new(CMD_INVALID, "for_window", "Found no name/value pairs in criteria");
		free_criteria(crit);
	} else if (list_seq_find(config->criteria, criteria_cmp, crit) != -1) {
		sway_log(L_DEBUG, "for_window: Duplicate, skipping.");
		free_criteria(crit);
	} else {
		sway_log(L_DEBUG, "for_window: '%s' -> '%s' added", crit->crit_raw, crit->cmdlist);
		list_add(config->criteria, crit);
	}
	return error ? error : cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_fullscreen(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "fullscreen", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "fullscreen", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "fullscreen", EXPECTED_AT_LEAST, 0))) {
		return error;
	}
	swayc_t *container = get_focused_view(&root_container);
	if(container->type != C_VIEW){
		return cmd_results_new(CMD_INVALID, "fullscreen", "Only views can fullscreen");
	}
	swayc_t *workspace = swayc_parent_by_type(container, C_WORKSPACE);
	bool current = swayc_is_fullscreen(container);
	wlc_view_set_state(container->handle, WLC_BIT_FULLSCREEN, !current);
	// Resize workspace if going from  fullscreen -> notfullscreen
	// otherwise just resize container
	if (!current) {
		arrange_windows(workspace, -1, -1);
		workspace->fullscreen = container;
	} else {
		arrange_windows(container, -1, -1);
		workspace->fullscreen = NULL;
	}
	ipc_event_window(container, "fullscreen_mode");

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (argc == 1 || (argc == 2 && strcasecmp(argv[0], "number") == 0) ) {
		if (config->reading || !config->active) {
			return cmd_results_new(CMD_DEFER, "workspace", NULL);
		}
		// Handle workspace next/prev
		swayc_t *ws = NULL;
		if (argc == 2) {
			if (!(ws = workspace_by_number(argv[1]))) {
				ws = workspace_create(argv[1]);
			}
		} else if (strcasecmp(argv[0], "next") == 0) {
			ws = workspace_next();
		} else if (strcasecmp(argv[0], "prev") == 0) {
			ws = workspace_prev();
		} else if (strcasecmp(argv[0], "next_on_output") == 0) {
			ws = workspace_output_next();
		} else if (strcasecmp(argv[0], "prev_on_output") == 0) {
			ws = workspace_output_prev();
		} else if (strcasecmp(argv[0], "back_and_forth") == 0) {
			if (prev_workspace_name) {
				if (!(ws = workspace_by_name(prev_workspace_name))) {
					ws = workspace_create(prev_workspace_name);
				}
			}
		} else {
			if (!(ws = workspace_by_name(argv[0]))) {
				ws = workspace_create(argv[0]);
			}
		}
		swayc_t *old_output = swayc_active_output();
		workspace_switch(ws);
		swayc_t *new_output = swayc_active_output();

		if (config->mouse_warping && old_output != new_output) {
			swayc_t *focused = get_focused_view(ws);
			if (focused && focused->type == C_VIEW) {
				center_pointer_on(focused);
			}
		}
	} else {
		if (strcasecmp(argv[1], "output") == 0) {
			if ((error = checkarg(argc, "workspace", EXPECTED_EQUAL_TO, 3))) {
				return error;
			}
			struct workspace_output *wso = calloc(1, sizeof(struct workspace_output));
			wso->workspace = strdup(argv[0]);
			wso->output = strdup(argv[2]);
			int i = -1;
			if ((i = list_seq_find(config->workspace_outputs, workspace_output_cmp_workspace, wso)) != -1) {
				struct workspace_output *old = config->workspace_outputs->items[i];
				free(old); // workspaces can only be assigned to a single output
				list_del(config->workspace_outputs, i);
			}
			sway_log(L_DEBUG, "Assigning workspace %s to output %s", argv[0], argv[2]);
			list_add(config->workspace_outputs, wso);
			if (!config->reading) {
				// TODO: Move workspace to output. (don't do so when reloading)
			}
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_workspace_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "default") == 0) {
		config->default_layout = L_NONE;
	} else if (strcasecmp(argv[0], "stacking") == 0) {
		config->default_layout = L_STACKED;
	} else if (strcasecmp(argv[0], "tabbed") == 0) {
		config->default_layout = L_TABBED;
	} else {
		return cmd_results_new(CMD_INVALID, "workspace_layout", "Expected 'workspace_layout <default|stacking|tabbed>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_ws_auto_back_and_forth(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_auto_back_and_forth", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "yes") == 0) {
		config->auto_back_and_forth = true;
	} else if (strcasecmp(argv[0], "no") == 0) {
		config->auto_back_and_forth = false;
	} else {
		return cmd_results_new(CMD_INVALID, "workspace_auto_back_and_forth", "Expected 'workspace_auto_back_and_forth <yes|no>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

/* Keep alphabetized */
static struct cmd_handler handlers[] = {
	{ "assign", cmd_assign },
	{ "bar", cmd_bar },
	{ "bindcode", cmd_bindcode },
	{ "bindsym", cmd_bindsym },
	{ "border", cmd_border },
	{ "client.background", cmd_client_background },
	{ "client.focused", cmd_client_focused },
	{ "client.focused_inactive", cmd_client_focused_inactive },
	{ "client.placeholder", cmd_client_placeholder },
	{ "client.unfocused", cmd_client_unfocused },
	{ "client.urgent", cmd_client_urgent },
	{ "debuglog", cmd_debuglog },
	{ "default_orientation", cmd_orientation },
	{ "exec", cmd_exec },
	{ "exec_always", cmd_exec_always },
	{ "exit", cmd_exit },
	{ "floating", cmd_floating },
	{ "floating_maximum_size", cmd_floating_maximum_size },
	{ "floating_minimum_size", cmd_floating_minimum_size },
	{ "floating_modifier", cmd_floating_mod },
	{ "floating_scroll", cmd_floating_scroll },
	{ "focus", cmd_focus },
	{ "focus_follows_mouse", cmd_focus_follows_mouse },
	{ "font", cmd_font },
	{ "for_window", cmd_for_window },
	{ "fullscreen", cmd_fullscreen },
	{ "gaps", cmd_gaps },
	{ "hide_edge_borders", cmd_hide_edge_borders },
	{ "include", cmd_include },
	{ "input", cmd_input },
	{ "kill", cmd_kill },
	{ "layout", cmd_layout },
	{ "log_colors", cmd_log_colors },
	{ "mode", cmd_mode },
	{ "mouse_warping", cmd_mouse_warping },
	{ "move", cmd_move },
	{ "new_float", cmd_new_float },
	{ "new_window", cmd_new_window },
	{ "output", cmd_output },
	{ "reload", cmd_reload },
	{ "resize", cmd_resize },
	{ "scratchpad", cmd_scratchpad },
	{ "seamless_mouse", cmd_seamless_mouse },
	{ "set", cmd_set },
	{ "smart_gaps", cmd_smart_gaps },
	{ "split", cmd_split },
	{ "splith", cmd_splith },
	{ "splitt", cmd_splitt },
	{ "splitv", cmd_splitv },
	{ "sticky", cmd_sticky },
	{ "workspace", cmd_workspace },
	{ "workspace_auto_back_and_forth", cmd_ws_auto_back_and_forth },
	{ "workspace_layout", cmd_workspace_layout },
};

static struct cmd_results *bar_cmd_binding_mode_indicator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "binding_mode_indicator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "binding_mode_indicator", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->binding_mode_indicator = true;
		sway_log(L_DEBUG, "Enabling binding mode indicator on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->binding_mode_indicator = false;
		sway_log(L_DEBUG, "Disabling binding mode indicator on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "binding_mode_indicator", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1))) {
		return error;
	} else if (!config->reading) {
		return cmd_results_new(CMD_FAILURE, "bindsym", "Can only be used in config file.");
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "bindsym", "No bar defined.");
	}

	if (strlen(argv[1]) != 7) {
		return cmd_results_new(CMD_INVALID, "bindsym", "Invalid mouse binding %s", argv[1]);
	}
	uint32_t numbutton = (uint32_t)atoi(argv[1] + 6);
	if (numbutton < 1 || numbutton > 5 || strncmp(argv[1], "button", 6) != 0) {
		return cmd_results_new(CMD_INVALID, "bindsym", "Invalid mouse binding %s", argv[1]);
	}
	struct sway_mouse_binding *binding = malloc(sizeof(struct sway_mouse_binding));
	binding->button = numbutton;
	binding->command = join_args(argv + 1, argc - 1);

	struct bar_config *bar = config->current_bar;
	int i = list_seq_find(bar->bindings, sway_mouse_binding_cmp_buttons, binding);
	if (i > -1) {
		sway_log(L_DEBUG, "bindsym - '%s' for swaybar already exists, overwriting", argv[0]);
		struct sway_mouse_binding *dup = bar->bindings->items[i];
		free_sway_mouse_binding(dup);
		list_del(bar->bindings, i);
	}
	list_add(bar->bindings, binding);
	list_qsort(bar->bindings, sway_mouse_binding_cmp_qsort);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s when clicking swaybar", argv[0], binding->command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_colors(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "colors", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcmp("{", argv[0]) != 0) {
		return cmd_results_new(CMD_INVALID, "colors",
				"Expected '{' at the start of colors config definition.");
	}

	return cmd_results_new(CMD_BLOCK_BAR_COLORS, NULL, NULL);
}

static struct cmd_results *bar_cmd_font(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "font", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "font", "No bar defined.");
	}

	char *font = join_args(argv, argc);
	free(config->current_bar->font);
	if (strlen(font) > 6 && strncmp("pango:", font, 6) == 0) {
		config->current_bar->font = font;
	} else {
		config->current_bar->font = font;
	}

	sway_log(L_DEBUG, "Settings font '%s' for bar: %s", config->current_bar->font, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_height(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "height", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	int height = atoi(argv[0]);
	if (height < 0) {
		return cmd_results_new(CMD_INVALID, "height",
				"Invalid height value: %s", argv[0]);
	}

	config->current_bar->height = height;
	sway_log(L_DEBUG, "Setting bar height to %d on bar: %s", height, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_set_hidden_state(struct bar_config *bar, const char *hidden_state) {
	char *old_state = bar->hidden_state;
	if (strcasecmp("toggle", hidden_state) == 0 && !config->reading) {
		if (strcasecmp("hide", bar->hidden_state) == 0) {
			bar->hidden_state = strdup("show");
		} else if (strcasecmp("show", bar->hidden_state) == 0) {
			bar->hidden_state = strdup("hide");
		}
	} else if (strcasecmp("hide", hidden_state) == 0) {
		bar->hidden_state = strdup("hide");
	} else if (strcasecmp("show", hidden_state) == 0) {
		bar->hidden_state = strdup("show");
	} else {
		return cmd_results_new(CMD_INVALID, "hidden_state", "Invalid value %s", hidden_state);
	}

	if (strcmp(old_state, bar->hidden_state) != 0) {
		if (!config->reading) {
			ipc_event_barconfig_update(bar);
		}
		sway_log(L_DEBUG, "Setting hidden_state: '%s' for bar: %s", bar->hidden_state, bar->id);
	}

	// free old mode
	free(old_state);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}


static struct cmd_results *bar_cmd_hidden_state(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hidden_state", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "hidden_state", EXPECTED_LESS_THAN, 3))) {
		return error;
	}

	if (config->reading && argc > 1) {
		return cmd_results_new(CMD_INVALID, "hidden_state", "Unexpected value %s in config mode", argv[1]);
	}

	const char *state = argv[0];

	if (config->reading) {
		return bar_set_hidden_state(config->current_bar, state);
	}

	const char *id = NULL;
	if (argc == 2) {
		id = argv[1];
	}

	int i;
	struct bar_config *bar;
	for (i = 0; i < config->bars->length; ++i) {
		bar = config->bars->items[i];
		if (id && strcmp(id, bar->id) == 0) {
			return bar_set_hidden_state(bar, state);
		}

		error = bar_set_hidden_state(bar, state);
		if (error) {
			return error;
		}
	}

	// active bar modifiers might have changed.
	update_active_bar_modifiers();

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_set_mode(struct bar_config *bar, const char *mode) {
	char *old_mode = bar->mode;
	if (strcasecmp("toggle", mode) == 0 && !config->reading) {
		if (strcasecmp("dock", bar->mode) == 0) {
			bar->mode = strdup("hide");
		} else if (strcasecmp("hide", bar->mode) == 0) {
			bar->mode = strdup("dock");
		}
	} else if (strcasecmp("dock", mode) == 0) {
		bar->mode = strdup("dock");
	} else if (strcasecmp("hide", mode) == 0) {
		bar->mode = strdup("hide");
	} else if (strcasecmp("invisible", mode) == 0) {
		bar->mode = strdup("invisible");
	} else {
		return cmd_results_new(CMD_INVALID, "mode", "Invalid value %s", mode);
	}

	if (strcmp(old_mode, bar->mode) != 0) {
		if (!config->reading) {
			ipc_event_barconfig_update(bar);

			// active bar modifiers might have changed.
			update_active_bar_modifiers();
		}
		sway_log(L_DEBUG, "Setting mode: '%s' for bar: %s", bar->mode, bar->id);
	}

	// free old mode
	free(old_mode);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "mode", EXPECTED_LESS_THAN, 3))) {
		return error;
	}

	if (config->reading && argc > 1) {
		return cmd_results_new(CMD_INVALID, "mode", "Unexpected value %s in config mode", argv[1]);
	}

	const char *mode = argv[0];

	if (config->reading) {
		return bar_set_mode(config->current_bar, mode);
	}

	const char *id = NULL;
	if (argc == 2) {
		id = argv[1];
	}

	int i;
	struct bar_config *bar;
	for (i = 0; i < config->bars->length; ++i) {
		bar = config->bars->items[i];
		if (id && strcmp(id, bar->id) == 0) {
			return bar_set_mode(bar, mode);
		}

		error = bar_set_mode(bar, mode);
		if (error) {
			return error;
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_id(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "id", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	const char *name = argv[0];
	const char *oldname = config->current_bar->id;

	// check if id is used by a previously defined bar
	int i;
	for (i = 0; i < config->bars->length; ++i) {
		struct bar_config *find = config->bars->items[i];
		if (strcmp(name, find->id) == 0 && config->current_bar != find) {
			return cmd_results_new(CMD_FAILURE, "id",
					"Id '%s' already defined for another bar. Id unchanged (%s).",
					name, oldname);
		}
	}

	sway_log(L_DEBUG, "Renaming bar: '%s' to '%s'", oldname, name);

	// free old bar id
	free(config->current_bar->id);

	config->current_bar->id = strdup(name);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_modifier(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "modifier", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "modifier", "No bar defined.");
	}

	uint32_t mod = 0;

	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		uint32_t tmp_mod;
		if ((tmp_mod = get_modifier_mask_by_name(split->items[i])) > 0) {
			mod |= tmp_mod;
			continue;
		} else {
			free_flat_list(split);
			return cmd_results_new(CMD_INVALID, "modifier", "Unknown modifier '%s'", split->items[i]);
		}
	}
	free_flat_list(split);

	config->current_bar->modifier = mod;
	sway_log(L_DEBUG, "Show/Hide the bar when pressing '%s' in hide mode.", argv[0]);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "output", "No bar defined.");
	}

	const char *output = argv[0];
	list_t *outputs = config->current_bar->outputs;
	if (!outputs) {
		outputs = create_list();
		config->current_bar->outputs = outputs;
	}

	int i;
	int add_output = 1;
	if (strcmp("*", output) == 0) {
		// remove all previous defined outputs and replace with '*'
		for (i = 0; i < outputs->length; ++i) {
			free(outputs->items[i]);
			list_del(outputs, i);
		}
	} else {
		// only add output if not already defined with either the same
		// name or as '*'
		for (i = 0; i < outputs->length; ++i) {
			const char *find = outputs->items[i];
			if (strcmp("*", find) == 0 || strcmp(output, find) == 0) {
				add_output = 0;
				break;
			}
		}
	}

	if (add_output) {
		list_add(outputs, strdup(output));
		sway_log(L_DEBUG, "Adding bar: '%s' to output '%s'", config->current_bar->id, output);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_position(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "position", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "position", "No bar defined.");
	}

	if (strcasecmp("top", argv[0]) == 0) {
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_TOP;
	} else if (strcasecmp("bottom", argv[0]) == 0) {
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	} else if (strcasecmp("left", argv[0]) == 0) {
		sway_log(L_INFO, "Warning: swaybar currently only supports top and bottom positioning. YMMV");
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_LEFT;
	} else if (strcasecmp("right", argv[0]) == 0) {
		sway_log(L_INFO, "Warning: swaybar currently only supports top and bottom positioning. YMMV");
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_RIGHT;
	} else {
		error = cmd_results_new(CMD_INVALID, "position", "Invalid value %s", argv[0]);
		return error;
	}

	sway_log(L_DEBUG, "Setting bar position '%s' for bar: %s", argv[0], config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_separator_symbol(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "separator_symbol", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "separator_symbol", "No bar defined.");
	}

	free(config->current_bar->separator_symbol);
	config->current_bar->separator_symbol = strdup(argv[0]);
	sway_log(L_DEBUG, "Settings separator_symbol '%s' for bar: %s", config->current_bar->separator_symbol, config->current_bar->id);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_status_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "status_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "status_command", "No bar defined.");
	}

	free(config->current_bar->status_command);
	config->current_bar->status_command = join_args(argv, argc);
	sway_log(L_DEBUG, "Feeding bar with status command: %s", config->current_bar->status_command);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_pango_markup(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pango_markup", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "pango_markup", "No bar defined.");
	}

	if (strcasecmp("enabled", argv[0]) == 0) {
		config->current_bar->pango_markup = true;
		sway_log(L_DEBUG, "Enabling pango markup for bar: %s", config->current_bar->id);
	} else if (strcasecmp("disabled", argv[0]) == 0) {
		config->current_bar->pango_markup = false;
		sway_log(L_DEBUG, "Disabling pango markup for bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "pango_markup", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_strip_workspace_numbers(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "strip_workspace_numbers", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "strip_workspace_numbers", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->strip_workspace_numbers = true;
		sway_log(L_DEBUG, "Stripping workspace numbers on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->strip_workspace_numbers = false;
		sway_log(L_DEBUG, "Enabling workspace numbers on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "strip_workspace_numbers", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_swaybar_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "swaybar_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "swaybar_command", "No bar defined.");
	}

	free(config->current_bar->swaybar_command);
	config->current_bar->swaybar_command = join_args(argv, argc);
	sway_log(L_DEBUG, "Using custom swaybar command: %s", config->current_bar->swaybar_command);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_tray_output(int argc, char **argv) {
	sway_log(L_ERROR, "Warning: tray_output is not supported on wayland");
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_tray_padding(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tray_padding", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "tray_padding", "No bar defined.");
	}

	int padding = atoi(argv[0]);
	if (padding < 0) {
		return cmd_results_new(CMD_INVALID, "tray_padding",
				"Invalid padding value %s, minimum is 0", argv[0]);
	}

	if (argc > 1 && strcasecmp("px", argv[1]) != 0) {
		return cmd_results_new(CMD_INVALID, "tray_padding",
				"Unknown unit %s", argv[1]);
	}
	config->current_bar->tray_padding = padding;
	sway_log(L_DEBUG, "Enabling tray padding of %d px on bar: %s", padding, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_wrap_scroll(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "wrap_scroll", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "wrap_scroll", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->wrap_scroll = true;
		sway_log(L_DEBUG, "Enabling wrap scroll on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->wrap_scroll = false;
		sway_log(L_DEBUG, "Disabling wrap scroll on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "wrap_scroll", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_cmd_workspace_buttons(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_buttons", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "workspace_buttons", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->workspace_buttons = true;
		sway_log(L_DEBUG, "Enabling workspace buttons on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->workspace_buttons = false;
		sway_log(L_DEBUG, "Disabling workspace buttons on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "workspace_buttons", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_handler bar_handlers[] = {
	{ "binding_mode_indicator", bar_cmd_binding_mode_indicator },
	{ "bindsym", bar_cmd_bindsym },
	{ "colors", bar_cmd_colors },
	{ "font", bar_cmd_font },
	{ "height", bar_cmd_height },
	{ "hidden_state", bar_cmd_hidden_state },
	{ "id", bar_cmd_id },
	{ "mode", bar_cmd_mode },
	{ "modifier", bar_cmd_modifier },
	{ "output", bar_cmd_output },
	{ "pango_markup", bar_cmd_pango_markup },
	{ "position", bar_cmd_position },
	{ "separator_symbol", bar_cmd_separator_symbol },
	{ "status_command", bar_cmd_status_command },
	{ "strip_workspace_numbers", bar_cmd_strip_workspace_numbers },
	{ "swaybar_command", bar_cmd_swaybar_command },
	{ "tray_output", bar_cmd_tray_output },
	{ "tray_padding", bar_cmd_tray_padding },
	{ "wrap_scroll", bar_cmd_wrap_scroll },
	{ "workspace_buttons", bar_cmd_workspace_buttons },
};

/**
 * Check and add color to buffer.
 *
 * return error object, or NULL if color is valid.
 */
static struct cmd_results *add_color(const char *name, char *buffer, const char *color) {
	int len = strlen(color);
	if (len != 7 && len != 9 ) {
		return cmd_results_new(CMD_INVALID, name, "Invalid color definition %s", color);
	}

	if (color[0] != '#') {
		return cmd_results_new(CMD_INVALID, name, "Invalid color definition %s", color);
	}

	int i;
	for (i = 1; i < len; ++i) {
		if (!isxdigit(color[i])) {
			return cmd_results_new(CMD_INVALID, name, "Invalid color definition %s", color);
		}
	}

	// copy color to buffer
	strncpy(buffer, color, len);
	// add default alpha channel if color was defined without it
	if (len == 7) {
		buffer[7] = 'f';
		buffer[8] = 'f';
	}

	return NULL;
}

static struct cmd_results *bar_colors_cmd_active_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "active_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("active_workspace_border", config->current_bar->colors.active_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("active_workspace_bg", config->current_bar->colors.active_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("active_workspace_text", config->current_bar->colors.active_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_background(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "background", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("background", config->current_bar->colors.background, argv[0]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_binding_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "binding_mode", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("binding_mode_border", config->current_bar->colors.binding_mode_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("binding_mode_bg", config->current_bar->colors.binding_mode_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("binding_mode_text", config->current_bar->colors.binding_mode_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_focused_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focused_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("focused_workspace_border", config->current_bar->colors.focused_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("focused_workspace_bg", config->current_bar->colors.focused_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("focused_workspace_text", config->current_bar->colors.focused_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_inactive_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "inactive_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("inactive_workspace_border", config->current_bar->colors.inactive_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("inactive_workspace_bg", config->current_bar->colors.inactive_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("inactive_workspace_text", config->current_bar->colors.inactive_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_separator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "separator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("separator", config->current_bar->colors.separator, argv[0]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_statusline(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "statusline", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("statusline", config->current_bar->colors.statusline, argv[0]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *bar_colors_cmd_urgent_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "urgent_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("urgent_workspace_border", config->current_bar->colors.urgent_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("urgent_workspace_bg", config->current_bar->colors.urgent_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("urgent_workspace_text", config->current_bar->colors.urgent_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_handler input_handlers[] = {
	{ "accel_profile", input_cmd_accel_profile },
	{ "click_method", input_cmd_click_method },
	{ "drag_lock", input_cmd_drag_lock },
	{ "dwt", input_cmd_dwt },
	{ "events", input_cmd_events },
	{ "middle_emulation", input_cmd_middle_emulation },
	{ "natural_scroll", input_cmd_natural_scroll },
	{ "pointer_accel", input_cmd_pointer_accel },
	{ "scroll_method", input_cmd_scroll_method },
	{ "tap", input_cmd_tap },
};

static struct cmd_handler bar_colors_handlers[] = {
	{ "active_workspace", bar_colors_cmd_active_workspace },
	{ "background", bar_colors_cmd_background },
	{ "binding_mode", bar_colors_cmd_binding_mode },
	{ "focused_workspace", bar_colors_cmd_focused_workspace },
	{ "inactive_workspace", bar_colors_cmd_inactive_workspace },
	{ "separator", bar_colors_cmd_separator },
	{ "statusline", bar_colors_cmd_statusline },
	{ "urgent_workspace", bar_colors_cmd_urgent_workspace },
};

static int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

static struct cmd_handler *find_handler(char *line, enum cmd_status block) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = NULL;
	sway_log(L_DEBUG, "find_handler(%s) %d", line, block == CMD_BLOCK_INPUT);
	if (block == CMD_BLOCK_BAR) {
		res = bsearch(&d, bar_handlers,
			sizeof(bar_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_BAR_COLORS){
		res = bsearch(&d, bar_colors_handlers,
			sizeof(bar_colors_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_INPUT) {
		sway_log(L_DEBUG, "lookng at input handlers");
		res = bsearch(&d, input_handlers,
			sizeof(input_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else {
		res = bsearch(&d, handlers,
			sizeof(handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	}
	return res;
}

struct cmd_results *handle_command(char *_exec) {
	// Even though this function will process multiple commands we will only
	// return the last error, if any (for now). (Since we have access to an
	// error string we could e.g. concatonate all errors there.)
	struct cmd_results *results = NULL;
	char *exec = strdup(_exec);
	char *head = exec;
	char *cmdlist;
	char *cmd;
	char *criteria __attribute__((unused));

	head = exec;
	do {
		// Extract criteria (valid for this command list only).
		criteria = NULL;
		if (*head == '[') {
			++head;
			criteria = argsep(&head, "]");
			if (head) {
				++head;
				// TODO handle criteria
			} else {
				if (!results) {
					results = cmd_results_new(CMD_INVALID, criteria, "Unmatched [");
				}
				goto cleanup;
			}
			// Skip leading whitespace
			head += strspn(head, whitespace);

			// TODO: it will yield unexpected results to execute commands
			// (on any view) that where meant for certain views only.
			if (!results) {
				int len = strlen(criteria) + strlen(head) + 4;
				char *tmp = malloc(len);
				snprintf(tmp, len, "[%s] %s", criteria, head);
				results = cmd_results_new(CMD_INVALID, tmp,
					"Can't handle criteria string: Refusing to execute command");
				free(tmp);
			}
			goto cleanup;
		}
		// Split command list
		cmdlist = argsep(&head, ";");
		cmdlist += strspn(cmdlist, whitespace);
		do {
			// Split commands
			cmd = argsep(&cmdlist, ",");
			cmd += strspn(cmd, whitespace);
			if (strcmp(cmd, "") == 0) {
				sway_log(L_INFO, "Ignoring empty command.");
				continue;
			}
			sway_log(L_INFO, "Handling command '%s'", cmd);
			//TODO better handling of argv
			int argc;
			char **argv = split_args(cmd, &argc);
			if (strcmp(argv[0], "exec") != 0) {
				int i;
				for (i = 1; i < argc; ++i) {
					if (*argv[i] == '\"' || *argv[i] == '\'') {
						strip_quotes(argv[i]);
					}
				}
			}
			struct cmd_handler *handler = find_handler(argv[0], CMD_BLOCK_END);
			if (!handler) {
				if (results) {
					free_cmd_results(results);
				}
				results = cmd_results_new(CMD_INVALID, cmd, "Unknown/invalid command");
				free_argv(argc, argv);
				goto cleanup;
			}
			struct cmd_results *res = handler->handle(argc-1, argv+1);
			if (res->status != CMD_SUCCESS) {
				free_argv(argc, argv);
				if (results) {
					free_cmd_results(results);
				}
				results = res;
				goto cleanup;
			}
			free_argv(argc, argv);
			free_cmd_results(res);
		} while(cmdlist);
	} while(head);
	cleanup:
	free(exec);
	if (!results) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return results;
}

// this is like handle_command above, except:
// 1) it ignores empty commands (empty lines)
// 2) it does variable substitution
// 3) it doesn't split commands (because the multiple commands are supposed to
//	  be chained together)
// 4) handle_command handles all state internally while config_command has some
//	  state handled outside (notably the block mode, in read_config)
struct cmd_results *config_command(char *exec, enum cmd_status block) {
	struct cmd_results *results = NULL;
	int argc;
	char **argv = split_args(exec, &argc);
	if (!argc) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
		goto cleanup;
	}

	sway_log(L_INFO, "handling config command '%s'", exec);
	// Endblock
	if (**argv == '}') {
		results = cmd_results_new(CMD_BLOCK_END, NULL, NULL);
		goto cleanup;
	}
	struct cmd_handler *handler = find_handler(argv[0], block);
	if (!handler) {
		char *input = argv[0] ? argv[0] : "(empty)";
		results = cmd_results_new(CMD_INVALID, input, "Unknown/invalid command");
		goto cleanup;
	}
	int i;
	// Var replacement, for all but first argument of set
	for (i = handler->handle == cmd_set ? 2 : 1; i < argc; ++i) {
		argv[i] = do_var_replacement(argv[i]);
		unescape_string(argv[i]);
	}
	/* Strip quotes for first argument.
	 * TODO This part needs to be handled much better */
	if (argc>1 && (*argv[1] == '\"' || *argv[1] == '\'')) {
		strip_quotes(argv[1]);
	}
	if (handler->handle) {
		results = handler->handle(argc-1, argv+1);
	} else {
		results = cmd_results_new(CMD_INVALID, argv[0], "This command is shimmed, but unimplemented");
	}
	cleanup:
	free_argv(argc, argv);
	return results;
}

struct cmd_results *cmd_results_new(enum cmd_status status, const char* input, const char *format, ...) {
	struct cmd_results *results = malloc(sizeof(struct cmd_results));
	results->status = status;
	if (input) {
		results->input = strdup(input); // input is the command name
	} else {
		results->input = NULL;
	}
	if (format) {
		char *error = malloc(256);
		va_list args;
		va_start(args, format);
		vsnprintf(error, 256, format, args);
		va_end(args);
		results->error = error;
	} else {
		results->error = NULL;
	}
	return results;
}

void free_cmd_results(struct cmd_results *results) {
	if (results->input) {
		free(results->input);
	}
	if (results->error) {
		free(results->error);
	}
	free(results);
}

const char *cmd_results_to_json(struct cmd_results *results) {
	json_object *root = json_object_new_object();
	json_object_object_add(root, "success", json_object_new_boolean(results->status == CMD_SUCCESS));
	if (results->input) {
		json_object_object_add(root, "input", json_object_new_string(results->input));
	}
	if (results->error) {
		json_object_object_add(root, "error", json_object_new_string(results->error));
	}
	const char *json = json_object_to_json_string(root);
	free(root);
	return json;
}
