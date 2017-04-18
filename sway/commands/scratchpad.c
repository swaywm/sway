#include <string.h>
#include <strings.h>
#include <wlc/wlc.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/focus.h"
#include "sway/layout.h"

static swayc_t *fetch_view_from_scratchpad() {
	if (sp_index >= scratchpad->length) {
		sp_index = 0;
	}
	swayc_t *view = scratchpad->items[sp_index++];

	if (wlc_view_get_output(view->handle) != swayc_active_output()->handle) {
		wlc_view_set_output(view->handle, swayc_active_output()->handle);
	}
	if (!view->is_floating) {
		view->width = swayc_active_workspace()->width * 0.5;
		view->height = swayc_active_workspace()->height * 0.75;
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

struct cmd_results *cmd_scratchpad(int argc, char **argv) {
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
