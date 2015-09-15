#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <wlc/wlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "stringop.h"
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "workspace.h"
#include "commands.h"
#include "container.h"
#include "handlers.h"
#include "sway.h"
#include "resize.h"

typedef enum cmd_status sway_cmd(int argc, char **argv);

struct cmd_handler {
	char *command;
	sway_cmd *handle;
};

static sway_cmd cmd_bindsym;
static sway_cmd cmd_orientation;
static sway_cmd cmd_exec;
static sway_cmd cmd_exec_always;
static sway_cmd cmd_exit;
static sway_cmd cmd_floating;
static sway_cmd cmd_floating_mod;
static sway_cmd cmd_focus;
static sway_cmd cmd_focus_follows_mouse;
static sway_cmd cmd_fullscreen;
static sway_cmd cmd_gaps;
static sway_cmd cmd_kill;
static sway_cmd cmd_layout;
static sway_cmd cmd_log_colors;
static sway_cmd cmd_mode;
static sway_cmd cmd_move;
static sway_cmd cmd_output;
static sway_cmd cmd_reload;
static sway_cmd cmd_resize;
static sway_cmd cmd_scratchpad;
static sway_cmd cmd_set;
static sway_cmd cmd_split;
static sway_cmd cmd_splith;
static sway_cmd cmd_splitv;
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

enum expected_args {
	EXPECTED_MORE_THAN,
	EXPECTED_AT_LEAST,
	EXPECTED_LESS_THAN,
	EXPECTED_EQUAL_TO
};

static bool checkarg(int argc, const char *name, enum expected_args type, int val) {
	switch (type) {
	case EXPECTED_MORE_THAN:
		if (argc > val) {
			return true;
		}
		sway_log(L_ERROR, "Invalid %s command."
			"(expected more than %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_AT_LEAST:
		if (argc >= val) {
			return true;
		}
		sway_log(L_ERROR, "Invalid %s command."
			"(expected at least %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_LESS_THAN:
		if (argc  < val) {
			return true;
		};
		sway_log(L_ERROR, "Invalid %s command."
			"(expected less than %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_EQUAL_TO:
		if (argc == val) {
			return true;
		};
		sway_log(L_ERROR, "Invalid %s command."
			"(expected %d arguments, got %d)", name, val, argc);
		break;
	}
	return false;
}

static int bindsym_sort(const void *_lbind, const void *_rbind) {
	const struct sway_binding *lbind = *(void **)_lbind;
	const struct sway_binding *rbind = *(void **)_rbind;
	unsigned int lmod = 0, rmod = 0, i;

	// Count how any modifiers are pressed
	for (i = 0; i < 8 * sizeof(lbind->modifiers); ++i) {
		lmod += lbind->modifiers & 1 << i;
		rmod += rbind->modifiers & 1 << i;
	}
	return (rbind->keys->length + rmod) - (lbind->keys->length + lmod);
}

static enum cmd_status cmd_bindsym(int argc, char **argv) {
	if (!checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1)
			|| !config->reading) {
		return CMD_FAILURE;
	};

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	int i;
	for (i = 0; i < split->length; ++i) {
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
			sway_log(L_ERROR, "bindsym - unknown key %s", (char *)split->items[i]);
			list_free(binding->keys);
			free(binding->command);
			free(binding);
			list_free(split);
			return CMD_FAILURE;
		}
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		*key = sym;
		list_add(binding->keys, key);
	}
	free_flat_list(split);

	// TODO: Check if there are other commands with this key binding
	struct sway_mode *mode = config->current_mode;
	list_add(mode->bindings, binding);
	list_sort(mode->bindings, bindsym_sort);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s", argv[0], binding->command);
	return CMD_SUCCESS;
}

static enum cmd_status cmd_exec_always(int argc, char **argv) {
	if (!config->active) return CMD_DEFER;;
	if (!checkarg(argc, "exec_always", EXPECTED_MORE_THAN, 0)) {
		return CMD_FAILURE;
	}
	// Put argument into cmd array
	char *tmp = join_args(argv, argc);
	char cmd[4096];
	strcpy(cmd, tmp);
	free(tmp);
	
	char *args[] = {"sh", "-c", cmd, 0 };

	pid_t pid = vfork();
	/* Failed to fork */
	if (pid  < 0) {
		sway_log(L_ERROR, "exec command failed, sway did not fork");
		return CMD_FAILURE;
	}
	/* Child process */
	if (pid == 0) {
		sway_log(L_DEBUG, "Executing %s", cmd);
		execv("/bin/sh", args);
		/* Execv doesnt return unless failure */
		sway_log(L_ERROR, "execv failde to return");
		_exit(-1);
	}
	/* Parent */
	return CMD_SUCCESS;
}

static enum cmd_status cmd_exec(int argc, char **argv) {
	if (!config->active) return CMD_DEFER;;

	if (config->reloading) {
		char *args = join_args(argv, argc);
		sway_log(L_DEBUG, "Ignoring 'exec %s' due to reload", args);
		free(args);
		return CMD_SUCCESS;
	}
	return cmd_exec_always(argc, argv);
}

static void kill_views(swayc_t *container, void *data) {
	if (container->type == C_VIEW) {
		wlc_view_close(container->handle);
	}
}

static enum cmd_status cmd_exit(int argc, char **argv) {
	if (!checkarg(argc, "exit", EXPECTED_EQUAL_TO, 0)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
	}
	// Close all views
	container_map(&root_container, kill_views, NULL);
	sway_terminate();
	return CMD_SUCCESS;
}

static enum cmd_status cmd_floating(int argc, char **argv) {
	if (!checkarg(argc, "floating", EXPECTED_EQUAL_TO, 1)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
	}

	if (strcasecmp(argv[0], "toggle") == 0) {
		swayc_t *view = get_focused_container(&root_container);
		// Prevent running floating commands on things like workspaces
		if (view->type != C_VIEW) {
			return CMD_SUCCESS;
		}
		// Change from nonfloating to floating
		if (!view->is_floating) {
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
		} else {
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
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_floating_mod(int argc, char **argv) {
	if (!checkarg(argc, "floating_modifier", EXPECTED_EQUAL_TO, 1)
			|| !config->reading) {
		return CMD_FAILURE;
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
		sway_log(L_ERROR, "bindsym - unknown keys %s", argv[0]);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_focus(int argc, char **argv) {
	static int floating_toggled_index = 0;
	static int tiled_toggled_index = 0;
	if (!checkarg(argc, "focus", EXPECTED_EQUAL_TO, 1)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
	}
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
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_focus_follows_mouse(int argc, char **argv) {
	if (!checkarg(argc, "focus_follows_mouse", EXPECTED_EQUAL_TO, 1)) {
		return CMD_FAILURE;
	}

	config->focus_follows_mouse = !strcasecmp(argv[0], "yes");
	return CMD_SUCCESS;
}

static void hide_view_in_scratchpad(swayc_t *sp_view) {
	if(sp_view == NULL) {
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

static enum cmd_status cmd_mode(int argc, char **argv) {
	if (!checkarg(argc, "mode", EXPECTED_AT_LEAST, 1)) {
		return CMD_FAILURE;
	}
	char *mode_name = join_args(argv, argc);
	int mode_len = strlen(mode_name);
	bool mode_make = mode_name[mode_len-1] == '{';
	if (mode_make) {
		if (!config->reading) return CMD_FAILURE;;
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
		sway_log(L_ERROR, "Unknown mode `%s'", mode_name);
		free(mode_name);
		return CMD_FAILURE;
	}
	if ((config->reading && mode_make) || (!config->reading && !mode_make)) {
		sway_log(L_DEBUG, "Switching to mode `%s'",mode->name);
	}
	free(mode_name);
	// Set current mode
	config->current_mode = mode;
	return mode_make ? CMD_BLOCK_MODE : CMD_SUCCESS;
}

static enum cmd_status cmd_move(int argc, char **argv) {
	if (config->reading) return CMD_FAILURE;;
	if (!checkarg(argc, "move", EXPECTED_AT_LEAST, 1)) {
		return CMD_FAILURE;
	}

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
		// "move container to workspace x"
		if (!checkarg(argc, "move container/window", EXPECTED_EQUAL_TO, 4)
				|| strcasecmp(argv[1], "to") != 0
				|| strcasecmp(argv[2], "workspace") != 0) {
			return CMD_FAILURE;
		}

		if (view->type != C_CONTAINER && view->type != C_VIEW) {
			return CMD_FAILURE;
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
	} else if (strcasecmp(argv[0], "scratchpad") == 0) {
		if (view->type != C_CONTAINER && view->type != C_VIEW) {
			return CMD_FAILURE;
		}
		swayc_t *view = get_focused_container(&root_container);
		int i;
		for (i = 0; i < scratchpad->length; i++) {
			if (scratchpad->items[i] == view) {
				hide_view_in_scratchpad(view);
				sp_view = NULL;
				return CMD_SUCCESS;
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
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_orientation(int argc, char **argv) {
	if (!config->reading) return CMD_FAILURE;;
	if (!checkarg(argc, "orientation", EXPECTED_EQUAL_TO, 1)) {
		return CMD_FAILURE;
	}

	if (strcasecmp(argv[0], "horizontal") == 0) {
		config->default_orientation = L_HORIZ;
	} else if (strcasecmp(argv[0], "vertical") == 0) {
		config->default_orientation = L_VERT;
	} else if (strcasecmp(argv[0], "auto") == 0) {
		// Do nothing
	} else {
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_output(int argc, char **argv) {
	if (!config->reading) return CMD_FAILURE;;
	if (!checkarg(argc, "output", EXPECTED_AT_LEAST, 1)) {
		return CMD_FAILURE;
	}

	struct output_config *output = calloc(1, sizeof(struct output_config));
	output->x = output->y = output->width = output->height = -1;
	output->name = strdup(argv[0]);
	output->enabled = true;

	// TODO: atoi doesn't handle invalid numbers
	if (strcasecmp(argv[1], "disable") == 0) {
		output->enabled = false;
	}

	int i;
	for (i = 1; i < argc; ++i) {
		if (strcasecmp(argv[i], "resolution") == 0 || strcasecmp(argv[i], "res") == 0) {
			char *res = argv[++i];
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
				res = argv[++i];
				height = atoi(res);
			}
			output->width = width;
			output->height = height;
		} else if (strcasecmp(argv[i], "position") == 0 || strcasecmp(argv[i], "pos") == 0) {
			char *res = argv[++i];
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
				res = argv[++i];
				y = atoi(res);
			}
			output->x = x;
			output->y = y;
		}
	}

	list_add(config->output_configs, output);

	sway_log(L_DEBUG, "Configured output %s to %d x %d @ %d, %d",
			output->name, output->width, output->height, output->x, output->y);

	return CMD_SUCCESS;
}

static enum cmd_status cmd_gaps(int argc, char **argv) {
	if (!checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1)) {
		return CMD_FAILURE;
	}
	const char *amount_str = argv[0];
	// gaps amount
	if (argc >= 1 && isdigit(*amount_str)) {
		int amount = (int)strtol(amount_str, NULL, 10);
		if (errno == ERANGE || amount == 0) {
			errno = 0;
			return CMD_FAILURE;
		}
		if (config->gaps_inner == 0) {
			config->gaps_inner = amount;
		}
		if (config->gaps_outer == 0) {
			config->gaps_outer = amount;
		}
		return CMD_SUCCESS;
	}
	// gaps inner|outer n
	else if (argc >= 2 && isdigit((amount_str = argv[1])[0])) {
		int amount = (int)strtol(amount_str, NULL, 10);
		if (errno == ERANGE || amount == 0) {
			errno = 0;
			return CMD_FAILURE;
		}
		const char *target_str = argv[0];
		if (strcasecmp(target_str, "inner") == 0) {
			config->gaps_inner = amount;
		} else if (strcasecmp(target_str, "outer") == 0) {
			config->gaps_outer = amount;
		}
		return CMD_SUCCESS;
	}
	// gaps inner|outer current|all set|plus|minus n
	if (argc < 4 || config->reading) {
		return CMD_FAILURE;
	}
	// gaps inner|outer ...
	const char *inout_str = argv[0];
	enum {INNER, OUTER} inout;
	if (strcasecmp(inout_str, "inner") == 0) {
		inout = INNER;
	} else if (strcasecmp(inout_str, "outer") == 0) {
		inout = OUTER;
	} else {
		return CMD_FAILURE;
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
		return CMD_FAILURE;
	}

	// gaps ... n
	amount_str = argv[3];
	int amount = (int)strtol(amount_str, NULL, 10);
	if (errno == ERANGE || amount == 0) {
		errno = 0;
		return CMD_FAILURE;
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
		return CMD_FAILURE;
	}

	if (target == CURRENT) {
		swayc_t *cont;
		if (inout == OUTER) {
			if ((cont = swayc_active_workspace()) == NULL) {
				return CMD_FAILURE;
			}
		} else {
			if ((cont = get_focused_view(&root_container))->type != C_VIEW) {
				return CMD_FAILURE;
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
				return CMD_FAILURE;
			}
		} else {
			top = &root_container;
		}
		int top_gap = top->gaps;
		container_map(top, method == SET ? set_gaps : add_gaps, &amount);
		top->gaps = top_gap;
		arrange_windows(top, -1, -1);
	}

	return CMD_SUCCESS;
}

static enum cmd_status cmd_kill(int argc, char **argv) {
	if (config->reading || !config->active) {
		return CMD_FAILURE;
	}
	swayc_t *view = get_focused_container(&root_container);
	wlc_view_close(view->handle);
	return CMD_SUCCESS;
}

static enum cmd_status cmd_layout(int argc, char **argv) {
	if (!checkarg(argc, "layout", EXPECTED_MORE_THAN, 0)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
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

	return CMD_SUCCESS;
}

static enum cmd_status cmd_reload(int argc, char **argv) {
	if (!checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0)
			|| config->reading
			|| !load_config(NULL)) {
		return CMD_FAILURE;
	}
	arrange_windows(&root_container, -1, -1);
	return CMD_SUCCESS;
}

static enum cmd_status cmd_resize(int argc, char **argv) {
	if (!checkarg(argc, "resize", EXPECTED_AT_LEAST, 3)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
	}
	char *end;
	int amount = (int)strtol(argv[2], &end, 10);
	if (errno == ERANGE || amount == 0) {
		errno = 0;
		return CMD_FAILURE;
	}

	if (strcmp(argv[0], "shrink") != 0 && strcmp(argv[0], "grow") != 0) {
		return CMD_FAILURE;
	}

	if (strcmp(argv[0], "shrink") == 0) {
		amount *= -1;
	}

	if (strcmp(argv[1], "width") == 0) {
		resize_tiled(amount, true);
	} else if (strcmp(argv[1], "height") == 0) {
		resize_tiled(amount, false);
	} else {
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
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

static enum cmd_status cmd_scratchpad(int argc, char **argv) {
	if (!checkarg(argc, "scratchpad", EXPECTED_EQUAL_TO, 1)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
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
		return CMD_SUCCESS;
	}
	return CMD_FAILURE;
}

// sort in order of longest->shortest
static int compare_set(const void *_l, const void *_r) {
	struct sway_variable * const *l = _l;
	struct sway_variable * const *r = _r;
	return strlen((*r)->name) - strlen((*l)->name);
}

static enum cmd_status cmd_set(int argc, char **argv) {
	if (!checkarg(argc, "set", EXPECTED_AT_LEAST, 2)
			|| !config->reading) {
		return CMD_FAILURE;
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
	return CMD_SUCCESS;
}

static enum cmd_status _do_split(int argc, char **argv, int layout) {
	char *name = layout == L_VERT  ? "splitv" :
		layout == L_HORIZ ? "splith" : "split";
	if (!checkarg(argc, name, EXPECTED_EQUAL_TO, 0)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
	}
	swayc_t *focused = get_focused_container(&root_container);

	// Case of floating window, dont split
	if (focused->is_floating) {
		return CMD_SUCCESS;
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
	return CMD_SUCCESS;
}

static enum cmd_status cmd_split(int argc, char **argv) {
	if (!checkarg(argc, "split", EXPECTED_EQUAL_TO, 1)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
	}

	if (strcasecmp(argv[0], "v") == 0 || strcasecmp(argv[0], "vertical") == 0) {
		_do_split(argc - 1, argv + 1, L_VERT);
	} else if (strcasecmp(argv[0], "h") == 0 || strcasecmp(argv[0], "horizontal") == 0) {
		_do_split(argc - 1, argv + 1, L_HORIZ);
	} else {
		sway_log(L_ERROR, "Invalid split command (expected either horiziontal or vertical).");
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_splitv(int argc, char **argv) {
	return _do_split(argc, argv, L_VERT);
}

static enum cmd_status cmd_splith(int argc, char **argv) {
	return _do_split(argc, argv, L_HORIZ);
}

static enum cmd_status cmd_log_colors(int argc, char **argv) {
	if (!checkarg(argc, "log_colors", EXPECTED_EQUAL_TO, 1)
			|| !config->reading) {
		return CMD_FAILURE;
	}
	if (strcasecmp(argv[0], "no") == 0) {
		sway_log_colors(0);
	} else if(strcasecmp(argv[0], "yes") == 0) {
		sway_log_colors(1);
	} else {
		sway_log(L_ERROR, "Invalid log_colors command (expected `yes` or `no`, got '%s')", argv[0]);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_fullscreen(int argc, char **argv) {
	if (!checkarg(argc, "fullscreen", EXPECTED_AT_LEAST, 0)
			|| config->reading || !config->active) {
		return CMD_FAILURE;
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

	return CMD_SUCCESS;
}

static enum cmd_status cmd_workspace(int argc, char **argv) {
	if (!checkarg(argc, "workspace", EXPECTED_AT_LEAST, 1)) {
		return CMD_FAILURE;
	}

	if (argc == 1) {
		if (config->reading || !config->active) {
			return CMD_DEFER;
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
		workspace_switch(ws);
	} else {
		if (strcasecmp(argv[1], "output") == 0) {
			if (!checkarg(argc, "workspace", EXPECTED_EQUAL_TO, 3)) {
				return CMD_FAILURE;
			}
			struct workspace_output *wso = calloc(1, sizeof(struct workspace_output));
			sway_log(L_DEBUG, "Assigning workspace %s to output %s", argv[0], argv[2]);
			wso->workspace = strdup(argv[0]);
			wso->output = strdup(argv[2]);
			list_add(config->workspace_outputs, wso);
			if (!config->reading) {
				// TODO: Move workspace to output. (dont do so when reloading)
			}
		}
	}
	return CMD_SUCCESS;
}

static enum cmd_status cmd_ws_auto_back_and_forth(int argc, char **argv) {
	if (!checkarg(argc, "workspace_auto_back_and_forth", EXPECTED_EQUAL_TO, 1)) {
		return CMD_FAILURE;
	}
	if (strcasecmp(argv[0], "yes") == 0) {
		config->auto_back_and_forth = true;
	} else if (strcasecmp(argv[0], "no") == 0) {
		config->auto_back_and_forth = false;
	} else {
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

/* Keep alphabetized */
static struct cmd_handler handlers[] = {
	{ "bindsym", cmd_bindsym },
	{ "default_orientation", cmd_orientation },
	{ "exec", cmd_exec },
	{ "exec_always", cmd_exec_always },
	{ "exit", cmd_exit },
	{ "floating", cmd_floating },
	{ "floating_modifier", cmd_floating_mod },
	{ "focus", cmd_focus },
	{ "focus_follows_mouse", cmd_focus_follows_mouse },
	{ "fullscreen", cmd_fullscreen },
	{ "gaps", cmd_gaps },
	{ "kill", cmd_kill },
	{ "layout", cmd_layout },
	{ "log_colors", cmd_log_colors },
	{ "mode", cmd_mode },
	{ "move", cmd_move },
	{ "output", cmd_output },
	{ "reload", cmd_reload },
	{ "resize", cmd_resize },
	{ "scratchpad", cmd_scratchpad },
	{ "set", cmd_set },
	{ "split", cmd_split },
	{ "splith", cmd_splith },
	{ "splitv", cmd_splitv },
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

enum cmd_status handle_command(char *_exec) {
	enum cmd_status status = CMD_SUCCESS;
	char *exec = strdup(_exec);
	char *head = exec;
	char *cmdlist;
	char *cmd;
	char *criteria __attribute__((unused));

	head = exec;
	do {
		// Handle criteria
		if (*head == '[') {
			criteria = argsep(&head, "]");
			if (head) {
				++head;
				// TODO handle criteria
			} else {
				sway_log(L_ERROR, "Unmatched [");
				status = CMD_INVALID;
			}
			// Skip leading whitespace
			head += strspn(head, whitespace);
		}
		// Split command list
		cmdlist = argsep(&head, ";");
		cmdlist += strspn(cmdlist, whitespace);
		do {
			// Split commands
			cmd = argsep(&cmdlist, ",");
			cmd += strspn(cmd, whitespace);
			sway_log(L_INFO, "Handling command '%s'", cmd);
			//TODO better handling of argv
			int argc;
			char **argv = split_args(cmd, &argc);
			if (argc>1 && (*argv[1] == '\"' || *argv[1] == '\'')) {
				strip_quotes(argv[1]);
			}
			struct cmd_handler *handler = find_handler(argv[0]);
			enum cmd_status res = CMD_INVALID;
			if (!handler
					|| (res = handler->handle(argc-1, argv+1)) != CMD_SUCCESS) {
				sway_log(L_ERROR, "Command '%s' failed", cmd);
				free_argv(argc, argv);
				status = res;
				goto cleanup;
			}
			free_argv(argc, argv);
		} while(cmdlist);
	} while(head);
	cleanup:
	free(exec);
	return status;
}

enum cmd_status config_command(char *exec) {
	sway_log(L_INFO, "handling config command '%s'", exec);
	enum cmd_status status = CMD_SUCCESS;
	int argc;
	char **argv = split_args(exec, &argc);
	if (!argc) {
		goto cleanup;
	}
	// Endblock
	if (**argv == '}') {
		status = CMD_BLOCK_END;
		goto cleanup;
	}
	struct cmd_handler *handler = find_handler(argv[0]);
	if (!handler) {
		status = CMD_INVALID;
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
	status = handler->handle(argc-1, argv+1);
	cleanup:
	free_argv(argc, argv);
	return status;
}
