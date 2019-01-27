sway-input(5)

# NAME

sway-input - input configuration file and commands

# DESCRIPTION

Sway allows for configuration of devices within the sway configuration file.
To obtain a list of available device identifiers, run *swaymsg -t get_inputs*.

# INPUT COMMANDS

## KEYBOARD CONFIGURATION

*input* <identifier> repeat_delay <n>
	Sets the delay before repeating a held-down key, in milliseconds.

*input* <identifier> repeat_rate <n>
	Sets the key repeat rate in number of keypresses per second.

For more information on these xkb configuration options, see
*xkeyboard-config*(7).

*input* <identifier> xkb_layout <layout_name>
	Sets the layout of the keyboard like _us_ or _de_.

*input* <identifier> xkb_model <model_name>
	Sets the model of the keyboard. This has an influence for some extra keys
	your keyboard might have.

*input* <identifier> xkb_options <options>
	Sets extra xkb configuration options for the keyboard.

*input* <identifier> xkb_rules <rules>
	Sets files of rules to be used for keyboard mapping composition.

*input* <identifier> xkb_variant <variant>
	Sets the variant of the keyboard like _dvorak_ or _colemak_.

The following commands may only be used in the configuration file.

*input* <identifier> xkb_capslock enabled|disabled
	Initially enables or disables CapsLock on startup, the default is disabled.

*input* <identifier> xkb_numlock enabled|disabled
	Initially enables or disables NumLock on startup, the default is disabled.

## MAPPING CONFIGURATION

*input* <identifier> map_to_output <identifier>
	Maps inputs from this device to the specified output. Only meaningful if the
	device is a pointer, touch, or drawing tablet device.

*input* <identifier> map_to_region <WxH@X,Y>
	Maps inputs from this device to the specified region of the global output
	layout. Only meaningful if the device is a pointer, touch, or drawing tablet
	device.

*input* <identifier> map_from_region <X1xY1> <X2xY2>
	Ignores inputs from this device that do not occur within the specified
	region. Can be in millimeters (e.g. 10x20mm 20x40mm) or in terms of 0..1
	(e.g. 0.5x0.5 0.7x0.7). Not all devices support millimeters. Only meaningful
	if the device is not a keyboard an provides events in absolute terms (such
	as a drawing tablet or touch screen - most pointers provide events relative
	to the previous frame).

## LIBINPUT CONFIGURATION

*input* <identifier> accel_profile adaptive|flat
	Sets the pointer acceleration profile for the specified input device.

*input* <identifier> click_method none|button_areas|clickfinger
	Changes the click method for the specified device.

*input* <identifier> drag enabled|disabled
	Enables or disables tap-and-drag for specified input device.

*input* <identifier> drag_lock enabled|disabled
	Enables or disables drag lock for specified input device.

*input* <identifier> dwt enabled|disabled
	Enables or disables disable-while-typing for the specified input device.

*input* <identifier> events enabled|disabled|disabled_on_external_mouse|toggle [<toggle-modes>]
	Enables or disables send_events for specified input device. Disabling
	send_events disables the input device.

	The _toggle_ option cannot be used in the config. If no toggle modes are
	listed, all supported modes for the device will be toggled through in the
	order: enabled,	disabled_on_external_mouse, disabled, (loop back). If
	toggle modes are listed, they will be cycled through, defaulting to the
	first mode listed if the current mode is not in the list. They will also
	not be checked to see if they are supported for the device and may fail.

*input* <identifier> left_handed enabled|disabled
	Enables or disables left handed mode for specified input device.

*input* <identifier> middle_emulation enabled|disabled
	Enables or disables middle click emulation.

*input* <identifier> natural_scroll enabled|disabled
	Enables or disables natural (inverted) scrolling for the specified input
	device.

*input* <identifier> pointer_accel [<-1|1>]
	Changes the pointer acceleration for the specified input device.

*input* <identifier> repeat_delay <milliseconds>
	Sets the amount of time a key must be held before it starts repeating.

*input* <identifier> repeat_rate <characters per second>
	Sets the frequency of key repeats once the repeat_delay has passed.

*input* <identifier> scroll_button disable|button[1-3,8,9]|<event-code-or-name>
	Sets the button used for scroll_method on_button_down. The button can
	be given as an event name or code, which can be obtained from `libinput
	debug-events`, or as a x11 mouse button (button[1-3,8,9]). If set to
	_disable_, it disables the scroll_method on_button_down.

*input* <identifier> scroll_factor <floating point value>
	Changes the scroll factor for the specified input device. Scroll speed will
	be scaled by the given value, which must be non-negative.

*input* <identifier> scroll_method none|two_finger|edge|on_button_down
	Changes the scroll method for the specified input device.

*input* <identifier> tap enabled|disabled
	Enables or disables tap for specified input device.

*input* <identifier> tap_button_map lrm|lmr
	Specifies which button mapping to use for tapping. _lrm_ treats 1 finger as
	left click, 2 fingers as right click, and 3 fingers as middle click. _lmr_
	treats 1 finger as left click, 2 fingers as middle click, and 3 fingers as
	right click.

## SEAT CONFIGURATION

Configure options for multiseat mode.

A *seat* is a collection of input devices that act independently of each other.
Seats are identified by name and the default seat is _seat0_ if no seats are
configured. Each seat has an independent keyboard focus and a separate cursor that
is controlled by the pointer devices of the seat. This is useful for multiple
people using the desktop at the same time with their own devices (each sitting
in their own "seat").

*seat* <name> attach <input_identifier>
	Attach an input device to this seat by its input identifier. A special
	value of "\*" will attach all devices to the seat.

*seat* <seat> cursor move|set <x> <y>
	Move specified seat's cursor relative to current position or wrap to
	absolute coordinates (with respect to the global coordinate space).
	Specifying either value as 0 will not update that coordinate.

*seat* <seat> cursor press|release button[1-9]|<event-name-or-code>
	Simulate pressing (or releasing) the specified mouse button on the
	specified seat. The button can either be provided as a button event name or
	event code, which can be obtained from `libinput debug-events`, or as an x11
	mouse button (button[1-9]). If using button[4-7], which map to axes, an axis
	event will be simulated, however _press_ and _release_ will be ignored and
	both will occur.

*seat* <name> fallback true|false
	Set this seat as the fallback seat. A fallback seat will attach any device
	not explicitly attached to another seat (similar to a "default" seat).

*seat* <name> hide_cursor <timeout>
	Hides the cursor image after the specified _timeout_ (in milliseconds)
	has elapsed with no activity on that cursor. A timeout of 0 (default)
	disables hiding the cursor. The minimal timeout is 100 and any value less
	than that (aside from 0), will be increased to 100.

# SEE ALSO

*sway*(5) *sway-output*(5)
