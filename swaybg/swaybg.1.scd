swaybg(1)

# NAME

swaybg - Screen background for Wayland

# SYNOPSIS

_swaybg_ [options...]

Displays a background in your Wayland session.

# OPTIONS

*-h, --help*
	Show help message and quit.

*-c, --color* <rrggbb[aa]>
	Turn the screen into the given color. If -i is used, this sets the
	background of the image to the given color. Defaults to white (FFFFFF), or
	transparent (00000000) if an image is in use.

*-i, --image* [<output>:]<path>
	Display the given image, optionally only on the given output. Use -c to set
	a background color.

*--scaling*
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, or _tile_.

*-v, --version*
	Show the version number and quit.

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about sway development, see
https://github.com/swaywm/sway.
