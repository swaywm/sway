sway(1)

# NAME

sway - SirCmpwn's Wayland window manager

# SYNOPSIS

*sway* [options...] [command]

# OPTIONS

*-h, --help*
	Show help message and quit.

*-c, --config* <config>
	Specifies a config file.

*-C, --validate*
	Check the validity of the config file, then exit.

*-d, --debug*
	Enables full logging, including debug information.

*-v, --version*
	Show the version number and quit.

*-V, --verbose*
	Enables more verbose logging.

*--get-socketpath*
	Gets the IPC socket path and prints it, then exits.

# DESCRIPTION

sway was created to fill the need of an i3-like window manager for Wayland. The
upstream i3 developers have no intention of porting i3 to Wayland, and projects
proposed by others ended up as vaporware. Many thanks to the i3 folks for
providing such a great piece of software, so good that your users would rather
write an entirely new window manager from scratch that behaved _exactly_ like i3
rather than switch to something else.

You can run sway directly from a tty, or via a Wayland-compatible login manager.

# CONFIGURATION

sway searches for a config file in the following locations, in this order:

- ~/.sway/config
- $XDG\_CONFIG\_HOME/sway/config (suggested location)
- ~/.i3/config
- $XDG\_CONFIG\_HOME/i3/config
- /etc/sway/config
- /etc/i3/config

If unset, $XDG\_CONFIG\_HOME defalts to *~/.config*.

An error is raised when no config file is found. The recommended default
configuration is usually installed to */etc/sway/config*; you are encouraged to
copy this to *~/.config/sway/config* and edit it from there.

For information on the config file format, see *sway*(5).

# IPC COMMANDS

Though *swaymsg*(1) is generally preferred, you may run *sway* _command_ to
send _command_ to the running instance of sway. You can also issue commands
with *i3-msg*(1) or even with *i3*(1).

# ENVIRONMENT

The following environment variables have an effect on sway:

_SWAY\_CURSOR\_THEME_
	Specifies the name of the cursor theme to use.

_SWAY\_CURSOR\_SIZE_
	Specifies the size of the cursor to use.

_SWAYSOCK_
	Specifies the path to the sway IPC socket.

_XKB\_DEFAULT\_RULES_, _XKB\_DEFAULT\_MODEL_, _XKB\_DEFAULT\_LAYOUT_,
_XKB\_DEFAULT\_VARIANT_, _XKB\_DEFAULT\_OPTIONS_
	Configures the xkb keyboard settings. See *xkeyboard-config*(7).

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about sway development, see
<https://github.com/swaywm/sway>.

# SEE ALSO

*sway*(5) *swaymsg*(1) *swaygrab*(1) *sway-input*(5) *sway-bar*(5)
