swaylock(1)

# NAME

swaylock - Screen locker for Wayland

# SYNOPSIS

_swaylock_ [options...]

Locks your Wayland session.

# OPTIONS

*-h, --help*
	Show help message and quit.

*-c, --color* <rrggbb[aa]>
	Turn the screen into the given color. If -i is used, this sets the
	background of the image to the given color. Defaults to white (FFFFFF), or
	transparent (00000000) if an image is in use.

*-f, --daemonize*
	Fork into the background after spawning. Note: this is the default behavior
	of i3lock.

*-i, --image* [<output>:]<path>
	Display the given image, optionally only on the given output. Use -c to set
	a background color.

*--scaling*
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, or _tile_.

*-t, --tiling*
	Same as --scaling=tile.

*-u, --no-unlock-indicator*
	Disable the unlock indicator.

*-f, --daemonize*
	Detach from the controlling terminal after locking.

*-v, --version*
	Show the version number and quit.

# APPEARANCE

*--bshlcolor* <rrggbb[aa]>
	Sets the color of backspace highlight segments.

*--font* <font>
	Sets the font of the text inside the indicator.

*--insidecolor* <rrggbb[aa]>
	Sets the color of the inside of the indicator when typing or idle.

*--insidevercolor* <rrggbb[aa]>
	Sets the color of the inside of the indicator when verifying.

*--insidewrongcolor* <rrggbb[aa]>
	Sets the color of the inside of the indicator when invalid.

*--keyhlcolor* <rrggbb[aa]>
	Sets the color of keypress highlight segments.

*--linecolor* <rrggbb[aa]>
	Sets the color of the lines that separate the inside and outside of the
	indicator.

*-s, --line-uses-inside*
	Use the color of the inside of the indicator for the line separating the
	inside and outside of the indicator.

*-r, --line-uses-ring*
	Use the outer ring's color for the line separating the inside and outside of
	the indicator.

*--ringcolor* <rrggbb[aa]>
	Sets the color of the outside of the indicator when typing or idle.

*--ringvercolor* <rrggbb[aa]>
	Sets the color of the outside of the indicator when verifying.

*--ringwrongcolor* <rrggbb[aa]>
	Sets the color of the outside of the indicator when invalid.

*--separatorcolor* <rrggbb[aa]>
	Sets the color of the lines that seperate highlight segments.

*--textcolor* <rrggbb[aa]>
	Sets the color of the text inside the indicator.

*--indicator-radius* <radius>
	Sets the radius of the indicator to _radius_ pixels. The default value is
	50.

*--indicator-thickness* <thickness>
	Sets the thickness of the indicator to _thickness_ pixels. The default value
	is 10.

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about sway development, see
https://github.com/swaywm/sway.
