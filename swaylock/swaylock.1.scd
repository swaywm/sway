swaylock(1)

# NAME

swaylock - Screen locker for Wayland

# SYNOPSIS

_swaylock_ [options...]

Locks your Wayland session.

# OPTIONS

*-C, --config* <path>
	The config file to use. By default, the following paths are checked:
	_$HOME/.swaylock/config_, _$XDG\_CONFIG\_HOME/swaylock/config_, and
	_SYSCONFDIR/swaylock/config_. All flags aside from this one are valid
	options in the configuration file using the format _long-option=value_.
	For options such as _ignore-empty-password_, just supply the _long-option_.
	All leading dashes should be omitted and the equals sign is required for
	flags that take an argument.

*-e, --ignore-empty-password*
	When an empty password is provided by the user, do not validate it.

*-f, --daemonize*
	Detach from the controlling terminal after locking.

	Note: this is the default behavior of i3lock.

*-h, --help*
	Show help message and quit.

*-v, --version*
	Show the version number and quit.

# APPEARANCE

*-u, --no-unlock-indicator*
	Disable the unlock indicator.

*-i, --image* [<output>:]<path>
	Display the given image, optionally only on the given output. Use -c to set
	a background color.

*-L, --disable-caps-lock-text*
	Disable the Caps Lock Text.

*-l, --indicator-caps-lock*
	Show the current Caps Lock state also on the indicator.

*-s, --scaling*
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, or _tile_.

*-t, --tiling*
	Same as --scaling=tile.

*-c, --color* <rrggbb[aa]>
	Turn the screen into the given color. If -i is used, this sets the
	background of the image to the given color. Defaults to white (FFFFFF), or
	transparent (00000000) if an image is in use.

*--bs-hl-color* <rrggbb[aa]>
	Sets the color of backspace highlight segments.

*--caps-lock-bs-hl-color* <rrggbb[aa]>
	Sets the color of backspace highlight segments when Caps Lock is active.

*--caps-lock-bs-hl-color* <rrggbb[aa]>
	Sets the color of the key press highlight segments when Caps Lock is active.

*--font* <font>
	Sets the font of the text inside the indicator.

*--indicator-radius* <radius>
	Sets the radius of the indicator to _radius_ pixels. The default value is
	50.

*--indicator-thickness* <thickness>
	Sets the thickness of the indicator to _thickness_ pixels. The default value
	is 10.

*--inside-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when typing or idle.

*--inside-clear-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when cleared.

*--inside-caps-lock-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when Caps Lock is active.

*--inside-ver-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when verifying.

*--inside-wrong-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when invalid.

*--key-hl-color* <rrggbb[aa]>
	Sets the color of key press highlight segments.

*--line-color* <rrggbb[aa]>
	Sets the color of the lines that separate the inside and outside of the
	indicator when typing or idle.

*--line-clear-color* <rrggbb[aa]>
	Sets the color of the lines that separate the inside and outside of the
	indicator when cleared.

*--line-caps-lock-color* <rrggbb[aa]>
	Sets the color of the line between the inside and ring when Caps Lock
	is active.

*--line-ver-color* <rrggbb[aa]>
	Sets the color of the lines that separate the inside and outside of the
	indicator when verifying.

*--line-wrong-color* <rrggbb[aa]>
	Sets the color of the lines that separate the inside and outside of the
	indicator when invalid.

*-n, --line-uses-inside*
	Use the color of the inside of the indicator for the line separating the
	inside and outside of the indicator.

*-r, --line-uses-ring*
	Use the outer ring's color for the line separating the inside and outside of
	the indicator.

*--ring-color* <rrggbb[aa]>
	Sets the color of the outside of the indicator when typing or idle.

*--ring-clear-color* <rrggbb[aa]>
	Sets the color of the outside of the indicator when cleared.

*--ring-caps-lock-color* <rrggbb[aa]>
	Sets the color of the ring of the indicator when Caps Lock is active.

*--ring-ver-color* <rrggbb[aa]>
	Sets the color of the outside of the indicator when verifying.

*--ring-wrong-color* <rrggbb[aa]>
	Sets the color of the outside of the indicator when invalid.

*--separator-color* <rrggbb[aa]>
	Sets the color of the lines that separate highlight segments.

*--text-color* <rrggbb[aa]>
	Sets the color of the text inside the indicator when typing or idle.

*--text-clear-color* <rrggbb[aa]>
	Sets the color of the text inside the indicator when cleared.

*--text-caps-lock-color* <rrggbb[aa]>
	Sets the color of the text when Caps Lock is active.

*--text-ver-color* <rrggbb[aa]>
	Sets the color of the text inside the indicator when verifying.

*--text-wrong-color* <rrggbb[aa]>
	Sets the color of the text inside the indicator when invalid.

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about sway development, see
https://github.com/swaywm/sway.
