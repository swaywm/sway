#include <limits.h>
#include <string.h>
#include <strings.h>
#include <wlr/config.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

#if WLR_HAS_LIBINPUT_BACKEND
#include <wlr/backend/libinput.h>
#endif

static void toggle_supported_send_events_for_device(struct input_config *ic,
		struct sway_input_device *input_device) {
#if WLR_HAS_LIBINPUT_BACKEND
	struct wlr_input_device *wlr_device = input_device->wlr_device;
	if (!wlr_input_device_is_libinput(wlr_device)) {
		return;
	}
	struct libinput_device *libinput_dev =
		wlr_libinput_get_device_handle(wlr_device);

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
#endif
}

static int mode_for_name(const char *name) {
	if (!strcmp(name, "enabled")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (!strcmp(name, "disabled_on_external_mouse")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else if (!strcmp(name, "disabled")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	}
	return -1;
}

static void toggle_select_send_events_for_device(struct input_config *ic,
		struct sway_input_device *input_device, int argc, char **argv) {
#if WLR_HAS_LIBINPUT_BACKEND
	if (!wlr_input_device_is_libinput(input_device->wlr_device)) {
		return;
	}
	// Get the currently set event mode since ic is a new config that will be
	// merged on the existing later. It should be set to INT_MIN before this.
	ic->send_events = libinput_device_config_send_events_get_mode(
			wlr_libinput_get_device_handle(input_device->wlr_device));

	int index;
	for (index = 0; index < argc; ++index) {
		if (mode_for_name(argv[index]) == ic->send_events) {
			++index;
			break;
		}
	}
	ic->send_events = mode_for_name(argv[index % argc]);
#endif
}

static void toggle_send_events(int argc, char **argv) {
	struct input_config *ic = config->handler_context.input_config;
	bool wildcard = strcmp(ic->identifier, "*") == 0;
	const char *type = strncmp(ic->identifier, "type:", strlen("type:")) == 0
		? ic->identifier + strlen("type:") : NULL;
	struct sway_input_device *device = NULL;
	wl_list_for_each(device, &server.input->devices, link) {
		if (wildcard || type) {
			ic = new_input_config(device->identifier);
			if (!ic) {
				continue;
			}
			if (type && strcmp(input_device_get_type(device), type) != 0) {
				continue;
			}
		} else if (strcmp(ic->identifier, device->identifier) != 0) {
			continue;
		}

		if (argc) {
			toggle_select_send_events_for_device(ic, device, argc, argv);
		} else {
			toggle_supported_send_events_for_device(ic, device);
		}

		if (wildcard || type) {
			store_input_config(ic, NULL);
		} else {
			return;
		}
	}
}

struct cmd_results *input_cmd_events(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "events", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (strcasecmp(argv[0], "enabled") == 0) {
		ic->send_events = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		ic->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	} else if (strcasecmp(argv[0], "disabled_on_external_mouse") == 0) {
		ic->send_events =
			LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else if (config->reading) {
		return cmd_results_new(CMD_INVALID,
			"Expected 'events <enabled|disabled|disabled_on_external_mouse>'");
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		for (int i = 1; i < argc; ++i) {
			if (mode_for_name(argv[i]) == -1) {
				return cmd_results_new(CMD_INVALID,
						"Invalid toggle mode %s", argv[i]);
			}
		}

		toggle_send_events(argc - 1, argv + 1);

		if (strcmp(ic->identifier, "*") == 0 ||
				strncmp(ic->identifier, "type:", strlen("type:")) == 0) {
			// Update the device input configs and then reset the type/wildcard
			// config send events mode so that is does not override the device
			// ones. The device ones will be applied when attempting to apply
			// the type/wildcard config
			ic->send_events = INT_MIN;
		}
	} else {
		return cmd_results_new(CMD_INVALID,
			"Expected 'events <enabled|disabled|disabled_on_external_mouse|"
			"toggle>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
