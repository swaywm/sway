sway-input(5)

# NAME

sway-input - input configuration file and commands

# DESCRIPTION

Sway allows for configuration of devices within the sway configuration file.
To obtain a list of available device identifiers, run *swaymsg -t get\_inputs*.

# INPUT COMMANDS

## KEYBOARD CONFIGURATION

*input* <identifier> repeat\_delay <n>
	Sets the delay before repeating a held-down key, in milliseconds.

*input* <identifier> repeat\_rate <n>
	Sets the key repeat rate in number of keypresses per second.

For more information on these xkb configuration options, see
*xkeyboard-config*(7).

*input* <identifier> xkb\_layout <layout\_name>
	Sets the layout of the keyboard like _us_ or _de_.

*input* <identifier> xkb\_model <model\_name>
	Sets the model of the keyboard. This has an influence for some extra keys
	your keyboard might have.

*input* <identifier> xkb\_options <options>
	Sets extra xkb configuration options for the keyboard.

*input* <identifier> xkb\_rules <rules>
	Sets files of rules to be used for keyboard mapping composition.

*input* <identifier> xkb\_variant <variant>
	Sets the variant of the keyboard like _dvorak_ or _colemak_.

The following commands may only be used in the configuration file.

*input* <identifier> xkb\_capslock enabled|disabled
	Initially enables or disables CapsLock on startup, the default is disabled.

*input* <identifier> xkb\_numlock enabled|disabled
	Initially enables or disables NumLock on startup, the default is disabled.

## MAPPING CONFIGURATION

*input* <identifier> map\_to\_output <identifier>
	Maps inputs from this device to the specified output. Only meaningful if the
	device is a pointer, touch, or drawing tablet device.

*input* <identifier> map\_to\_region <WxH@X,Y>
	Maps inputs from this device to the specified region of the global output
	layout. Only meaningful if the device is a pointer, touch, or drawing tablet
	device.

*input* <identifier> map\_from\_region <X1xY1> <X2xY2>
	Ignores inputs from this device that do not occur within the specified
	region. Can be in millimeters (e.g. 10x20mm 20x40mm) or in terms of 0..1
	(e.g. 0.5x0.5 0.7x0.7). Not all devices support millimeters. Only meaningful
	if the device is not a keyboard an provides events in absolute terms (such
	as a drawing tablet or touch screen - most pointers provide events relative
	to the previous frame).

## LIBINPUT CONFIGURATION

*input* <identifier> accel\_profile adaptive|flat
	Sets the pointer acceleration profile for the specified input device.

*input* <identifier> click\_method none|button\_areas|clickfinger
	Changes the click method for the specified device.

*input* <identifier> drag enabled|disabled
	Enables or disables tap-and-drag for specified input device.

*input* <identifier> drag\_lock enabled|disabled
	Enables or disables drag lock for specified input device.

*input* <identifier> dwt enabled|disabled
	Enables or disables disable-while-typing for the specified input device.

*input* <identifier> events enabled|disabled|disabled\_on\_external\_mouse
	Enables or disables send\_events for specified input device. (Disabling
	send\_events disables the input device)

*input* <identifier> left\_handed enabled|disabled
	Enables or disables left handed mode for specified input device.

*input* <identifier> middle\_emulation enabled|disabled
	Enables or disables middle click emulation.

*input* <identifier> natural\_scroll enabled|disabled
	Enables or disables natural (inverted) scrolling for the specified input
	device.

*input* <identifier> pointer\_accel [<-1|1>]
	Changes the pointer acceleration for the specified input device.

*input* <identifier> repeat\_delay <milliseconds>
	Sets the amount of time a key must be held before it starts repeating.

*input* <identifier> repeat\_rate <characters per second>
	Sets the frequency of key repeats once the repeat\_delay has passed.

*input* <identifier> scroll\_button <button\_identifier>
	Sets button used for scroll\_method on\_button\_down. The button identifier
	can be obtained from `libinput debug-events`.
	If set to 0, it disables the scroll\_button on\_button\_down.

*input* <identifier> scroll\_factor <floating point value>
	Changes the scroll factor for the specified input device. Scroll speed will
	be scaled by the given value, which must be non-negative.

*input* <identifier> scroll\_method none|two\_finger|edge|on\_button\_down
	Changes the scroll method for the specified input device.

*input* <identifier> tap enabled|disabled
	Enables or disables tap for specified input device.

*input* <identifier> tap\_button\_map lrm|lmr
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

*seat* <name> attach <input\_identifier>
	Attach an input device to this seat by its input identifier. A special
	value of "\*" will attach all devices to the seat.

*seat* <name> fallback true|false
	Set this seat as the fallback seat. A fallback seat will attach any device
	not explicitly attached to another seat (similar to a "default" seat).

# SEE ALSO

*sway*(5) *sway-output*(5)
