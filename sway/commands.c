#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <wlc/wlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "stringop.h"
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "workspace.h"
#include "commands.h"
#include "container.h"
#include "output.h"
#include "handlers.h"
#include "sway.h"
#include "resize.h"
#include "input_state.h"
#include "criteria.h"

typedef struct cmd_results *sway_cmd(int argc, char **argv);

struct cmd_handler {
	char *command;
	sway_cmd *handle;
};

static sway_cmd cmd_bindsym;
static sway_cmd cmd_debuglog;
static sway_cmd cmd_exec;
static sway_cmd cmd_exec_always;
static sway_cmd cmd_exit;
static sway_cmd cmd_floating;
static sway_cmd cmd_floating_mod;
static sway_cmd cmd_focus;
static sway_cmd cmd_focus_follows_mouse;
static sway_cmd cmd_for_window;
static sway_cmd cmd_fullscreen;
static sway_cmd cmd_gaps;
static sway_cmd cmd_kill;
static sway_cmd cmd_layout;
static sway_cmd cmd_log_colors;
static sway_cmd cmd_mode;
static sway_cmd cmd_mouse_warping;
static sway_cmd cmd_move;
static sway_cmd cmd_orientation;
static sway_cmd cmd_output;
static sway_cmd cmd_reload;
static sway_cmd cmd_resize;
static sway_cmd cmd_scratchpad;
static sway_cmd cmd_set;
static sway_cmd cmd_split;
static sway_cmd cmd_splith;
static sway_cmd cmd_splitv;
static sway_cmd cmd_sticky;
static sway_cmd cmd_workspace;
static sway_cmd cmd_ws_auto_back_and_forth;

swayc_t *sp_view;
int sp_index = 0;

static struct modifier_key {
	char *name;
	uint32_t mod;
} modifiers[] = {
	{ XKB_MOD_NAME_SHIFT, WLC_BIT_MOD_SHIFT },
	{ XKB_MOD_NAME_CAPS, WLC_BIT_MOD_CAPS },
	{ XKB_MOD_NAME_CTRL, WLC_BIT_MOD_CTRL },
	{ "Ctrl", WLC_BIT_MOD_CTRL },
	{ XKB_MOD_NAME_ALT, WLC_BIT_MOD_ALT },
	{ "Alt", WLC_BIT_MOD_ALT },
	{ XKB_MOD_NAME_NUM, WLC_BIT_MOD_MOD2 },
	{ "Mod3", WLC_BIT_MOD_MOD3 },
	{ XKB_MOD_NAME_LOGO, WLC_BIT_MOD_LOGO },
	{ "Mod5", WLC_BIT_MOD_MOD5 },
};

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

static struct cmd_results *cmd_bindsym(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1))) {
		return error;
	} else if (!config->reading) {
		return cmd_results_new(CMD_FAILURE, "bindsym", "Can only be used in config file.");
	}

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; ++i) {
		// Check for a modifier key
		int j;
		bool is_mod = false;
		for (j = 0; j < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++j) {
			if (strcasecmp(modifiers[j].name, split->items[i]) == 0) {
				binding->modifiers |= modifiers[j].mod;
				is_mod = true;
				break;
			}
		}
		if (is_mod) continue;
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
	list_add(mode->bindings, binding);
	list_sort(mode->bindings, sway_binding_cmp);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s", argv[0], binding->command);
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
	strcpy(cmd, tmp);
	free(tmp);
	sway_log(L_DEBUG, "Executing %s", cmd);

	int fd[2];
	pipe(fd);

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
		write(fd[1], child, sizeof(pid_t));
		close(fd[1]);
		_exit(0); // Close child process
	} else if (pid < 0) {
		return cmd_results_new(CMD_FAILURE, "exec_always", "Command failed (sway could not fork).");
	}
	close(fd[1]); // close write
	read(fd[0], child, sizeof(pid_t));
	close(fd[0]);
	// cleanup child process
	wait(0);
	if (*child > 0) {
		sway_log(L_DEBUG, "Child process created with pid %d", *child);
		// TODO: keep track of this pid and open the corresponding view on the current workspace
		// blocked pending feature in wlc
	}
	free(child);
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

static void kill_views(swayc_t *container, void *data) {
	if (container->type == C_VIEW) {
		wlc_view_close(container->handle);
	}
}

static struct cmd_results *cmd_exit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "exit", "Can't be used in config file.");
	if ((error = checkarg(argc, "exit", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	// Close all views
	container_map(&root_container, kill_views, NULL);
	sway_terminate();
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
		// Using length-1 as the index is safe because the view must be the currently
		// focused floating output
		remove_child(view);
		view->is_floating = false;
		// Get the properly focused container, and add in the view there
		swayc_t *focused = container_under_pointer();
		// If focused is null, it's because the currently focused container is a workspace
		if (focused == NULL) {
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
	}
	set_focused_container(view);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_floating_mod(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_modifier", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	int i, j;
	list_t *split = split_string(argv[0], "+");
	config->floating_mod = 0;

	// set modifer keys
	for (i = 0; i < split->length; ++i) {
		for (j = 0; j < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++j) {
			if (strcasecmp(modifiers[j].name, split->items[i]) == 0) {
				config->floating_mod |= modifiers[j].mod;
			}
		}
	}
	free_flat_list(split);
	if (!config->floating_mod) {
		error = cmd_results_new(CMD_INVALID, "floating_modifier", "Unknown keys %s", argv[0]);
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
	}
	arrange_windows(ws, -1, -1);
	set_focused_container(container_under_pointer());
}

static struct cmd_results *cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	char *mode_name = join_args(argv, argc);
	int mode_len = strlen(mode_name);
	bool mode_make = mode_name[mode_len-1] == '{';
	if (mode_make) {
		if (!config->reading)
			return cmd_results_new(CMD_FAILURE, "mode", "Can only be used in config file.");
		// Trim trailing spaces
		do {
			mode_name[--mode_len] = 0;
		} while(isspace(mode_name[mode_len-1]));
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
	// Create mode if it doesnt exist
	if (!mode && mode_make) {
		mode = malloc(sizeof*mode);
		mode->name = strdup(mode_name);
		mode->bindings = create_list();
		list_add(config->modes, mode);
	}
	if (!mode) {
		error = cmd_results_new(CMD_INVALID, "mode", "Unknown mode `%s'", mode_name);
		free(mode_name);
		return error;
	}
	if ((config->reading && mode_make) || (!config->reading && !mode_make)) {
		sway_log(L_DEBUG, "Switching to mode `%s'",mode->name);
	}
	free(mode_name);
	// Set current mode
	config->current_mode = mode;
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
		"'move <container|window|workspace> to output <name|direction>'";
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
			if (argc == 5) {
				// move "container to workspace number x"
				ws_name = argv[4];
			}

			swayc_t *ws = workspace_by_name(ws_name);
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
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}
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

int output_name_cmp(const void *item, const void *data)
{
	const struct output_config *output = item;
	const char *name = data;

	return strcmp(output->name, name);
}

static struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	const char *name = argv[0];
	const char *command = argv[1];

	struct output_config *output;
	int index = list_seq_find(config->output_configs, output_name_cmp, name);
	if (index >= 0) {
		output = config->output_configs->items[index];
	} else {
		output = calloc(1, sizeof(struct output_config));
		output->x = output->y = output->width = output->height = -1;
		output->name = strdup(name);
		output->enabled = true;
	}

	// TODO: atoi doesn't handle invalid numbers
	if (strcasecmp(command, "disable") == 0) {
		output->enabled = false;
	}
	// TODO: Check missing params after each sub-command

	if (strcasecmp(command, "resolution") == 0 || strcasecmp(command, "res") == 0) {
		char *res = argv[2];
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
			res = argv[3];
			height = atoi(res);
		}
		output->width = width;
		output->height = height;
	} else if (strcasecmp(command, "position") == 0 || strcasecmp(command, "pos") == 0) {
		char *res = argv[2];
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
			res = argv[3];
			y = atoi(res);
		}
		output->x = x;
		output->y = y;
	} else if (strcasecmp(command, "bg") == 0 || strcasecmp(command, "background") == 0) {
		wordexp_t p;
		char *src = argv[2];
		char *mode = argv[3];
		if (wordexp(src, &p, 0) != 0) {
			return cmd_results_new(CMD_INVALID, "output", "Invalid syntax (%s)", src);
		}
		src = p.we_wordv[0];
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
		wordfree(&p);
	}

	int i = 1;
	for (i = 0; i < config->output_configs->length; ++i) {
		struct output_config *oc = config->output_configs->items[i];
		if (strcmp(oc->name, output->name) == 0) {
			// replace existing config
			list_del(config->output_configs, i);
			free_output_config(oc);
			break;
		}
	}
	if (index == -1) {
		list_add(config->output_configs, output);
	}

	sway_log(L_DEBUG, "Config stored for output %s (%d x %d @ %d, %d) (bg %s %s)",
			output->name, output->width, output->height, output->x, output->y,
			output->background, output->background_option);

	if (output->name) {
		// Try to find the output container and apply configuration now. If
		// this is during startup then there will be no container and config
		// will be applied during normal "new output" event from wlc.
		swayc_t *cont = NULL;
		for (int i = 0; i < root_container.children->length; ++i) {
			cont = root_container.children->items[i];
			if (cont->name && strcmp(cont->name, output->name) == 0) {
				apply_output_config(output, cont);
				break;
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
	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	if (strcasecmp(argv[0], "splith") == 0) {
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
	arrange_windows(parent, parent->width, parent->height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_reload(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "reload", "Can't be used in config file.");
	if ((error = checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	if (!load_config(NULL)) return cmd_results_new(CMD_FAILURE, "reload", "Error(s) reloading config.");

	arrange_windows(&root_container, -1, -1);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_resize(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "resize", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "resize", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 3))) {
		return error;
	}
	char *end;
	int amount = (int)strtol(argv[2], &end, 10);
	if (errno == ERANGE || amount == 0) {
		errno = 0;
		return cmd_results_new(CMD_INVALID, "resize", "Number is out of range.");
	}

	if (strcmp(argv[0], "shrink") != 0 && strcmp(argv[0], "grow") != 0) {
		return cmd_results_new(CMD_INVALID, "resize",
			"Expected 'resize <shrink|grow> <width|height> <amount>'");
	}

	if (strcmp(argv[0], "shrink") == 0) {
		amount *= -1;
	}

	if (strcmp(argv[1], "width") == 0) {
		resize_tiled(amount, true);
	} else if (strcmp(argv[1], "height") == 0) {
		resize_tiled(amount, false);
	} else {
		return cmd_results_new(CMD_INVALID, "resize",
			"Expected 'resize <shrink|grow> <width|height> <amount>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
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
static int compare_set(const void *_l, const void *_r) {
	struct sway_variable const *l = _l;
	struct sway_variable const *r = _r;
	return strlen(r->name) - strlen(l->name);
}

static struct cmd_results *cmd_set(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->reading) return cmd_results_new(CMD_FAILURE, "set", "Can only be used in config file.");
	if ((error = checkarg(argc, "set", EXPECTED_AT_LEAST, 2))) {
		return error;
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
		list_sort(config->symbols, compare_set);
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

	// Case of floating window, dont split
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
		swayc_t *parent = new_container(focused, layout);
		set_focused_container(focused);
		arrange_windows(parent, -1, -1);
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
	} else {
		error = cmd_results_new(CMD_FAILURE, "split",
			"Invalid split command (expected either horiziontal or vertical).");
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
	bool current = swayc_is_fullscreen(container);
	wlc_view_set_state(container->handle, WLC_BIT_FULLSCREEN, !current);
	// Resize workspace if going from  fullscreen -> notfullscreen
	// otherwise just resize container
	if (current) {
		container = swayc_parent_by_type(container, C_WORKSPACE);
	}
	// Only resize container when going into fullscreen
	arrange_windows(container, -1, -1);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (argc == 1) {
		if (config->reading || !config->active) {
			return cmd_results_new(CMD_DEFER, "workspace", NULL);
		}
		// Handle workspace next/prev
		swayc_t *ws = NULL;
		if (strcasecmp(argv[0], "next") == 0) {
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
			if (!(ws= workspace_by_name(argv[0]))) {
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
				// TODO: Move workspace to output. (dont do so when reloading)
			}
		}
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
	{ "bindsym", cmd_bindsym },
	{ "debuglog", cmd_debuglog },
	{ "default_orientation", cmd_orientation },
	{ "exec", cmd_exec },
	{ "exec_always", cmd_exec_always },
	{ "exit", cmd_exit },
	{ "floating", cmd_floating },
	{ "floating_modifier", cmd_floating_mod },
	{ "focus", cmd_focus },
	{ "focus_follows_mouse", cmd_focus_follows_mouse },
	{ "for_window", cmd_for_window },
	{ "fullscreen", cmd_fullscreen },
	{ "gaps", cmd_gaps },
	{ "kill", cmd_kill },
	{ "layout", cmd_layout },
	{ "log_colors", cmd_log_colors },
	{ "mode", cmd_mode },
	{ "mouse_warping", cmd_mouse_warping },
	{ "move", cmd_move },
	{ "output", cmd_output },
	{ "reload", cmd_reload },
	{ "resize", cmd_resize },
	{ "scratchpad", cmd_scratchpad },
	{ "seamless_mouse", cmd_seamless_mouse },
	{ "set", cmd_set },
	{ "split", cmd_split },
	{ "splith", cmd_splith },
	{ "splitv", cmd_splitv },
	{ "sticky", cmd_sticky },
	{ "workspace", cmd_workspace },
	{ "workspace_auto_back_and_forth", cmd_ws_auto_back_and_forth },
};

static int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

static struct cmd_handler *find_handler(char *line) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = bsearch(&d, handlers,
			sizeof(handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
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
			if (argc>1 && (*argv[1] == '\"' || *argv[1] == '\'')) {
				strip_quotes(argv[1]);
			}
			struct cmd_handler *handler = find_handler(argv[0]);
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
struct cmd_results *config_command(char *exec) {
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
	struct cmd_handler *handler = find_handler(argv[0]);
	if (!handler) {
		char *input = argv[0] ? argv[0] : "(empty)";
		results = cmd_results_new(CMD_INVALID, input, "Unknown/invalid command");
		goto cleanup;
	}
	int i;
	// Var replacement, for all but first argument of set
	for (i = handler->handle == cmd_set ? 2 : 1; i < argc; ++i) {
		argv[i] = do_var_replacement(argv[i]);
	}
	/* Strip quotes for first argument.
	 * TODO This part needs to be handled much better */
	if (argc>1 && (*argv[1] == '\"' || *argv[1] == '\'')) {
		strip_quotes(argv[1]);
	}
	results = handler->handle(argc-1, argv+1);
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
