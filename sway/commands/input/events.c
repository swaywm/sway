#include <limits.h>
#include <string.h>
#include <strings.h>
#include <wlr/backend/libinput.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

static void toggle_send_events_for_device(struct input_config *ic,
		struct sway_input_device *input_device) {
	struct wlr_input_device *wlr_device = input_device->wlr_device;
	if (!wlr_input_device_is_libinput(wlr_device)) {
		return;
	}
	struct libinput_device *libinput_dev
		= wlr_libinput_get_device_handle(wlr_device);

	enum libinput_config_send_events_mode mode =
		libinput_device_config_send_events_get_mode(libinput_dev);
	uint32_t possible =
		libinput_device_config_send_events_get_modes(libinput_dev);

	switch (mode) {
	case LIBINPUT_CONFIG_SEND_EVENTS_ENABLED:
		mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
		if (possible & mode) {
			break;
		}
		// fall through
	case LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE:
		mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
		if (possible & mode) {
			break;
		}
		// fall through
	case LIBINPUT_CONFIG_SEND_EVENTS_DISABLED:
	default:
		mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
		break;
	}

	ic->send_events = mode;
}

static void toggle_send_events(struct input_config *ic) {
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &server.input->devices, link) {
		if (strcmp(input_device->identifier, ic->identifier) == 0) {
			toggle_send_events_for_device(ic, input_device);
		}
	}
}

static void toggle_wildcard_send_events() {
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &server.input->devices, link) {
		struct input_config *ic = new_input_config(input_device->identifier);
		if (!ic) {
			break;
		}
		toggle_send_events_for_device(ic, input_device);
		store_input_config(ic);
	}
}

struct cmd_results *input_cmd_events(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "events", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "events",
			"No input device defined.");
	}

	if (strcasecmp(argv[0], "enabled") == 0) {
		ic->send_events = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		ic->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	} else if (strcasecmp(argv[0], "disabled_on_external_mouse") == 0) {
		ic->send_events =
			LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else if (config->reading) {
		return cmd_results_new(CMD_INVALID, "events",
			"Expected 'events <enabled|disabled|disabled_on_external_mouse>'");
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		if (strcmp(ic->identifier, "*") == 0) {
			// Update the device input configs and then reset the wildcard
			// config send events mode so that is does not override the device
			// ones. The device ones will be applied when attempting to apply
			// the wildcard config
			toggle_wildcard_send_events();
			ic->send_events = INT_MIN;
		} else {
			toggle_send_events(ic);
		}
	} else {
		return cmd_results_new(CMD_INVALID, "events",
			"Expected 'events <enabled|disabled|disabled_on_external_mouse|"
			"toggle>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
