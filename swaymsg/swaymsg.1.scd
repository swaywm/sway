swaymsg(1)

# NAME

swaymsg - Send messages to a running instance of sway over the IPC socket.

# SYNOPSIS

_swaymsg_ [options...] [message]

# OPTIONS

*-h, --help*
	Show help message and quit.

*-q, --quiet*
	Sends the IPC message but does not print the response from sway.

*-r, --raw*
	Use raw output even if using a tty.

*-s, --socket* <path>
	Use the specified socket path. Otherwise, swaymsg will ask sway where the
	socket is (which is the value of $SWAYSOCK, then of $I3SOCK).

*-t, --type* <type>
	Specify the type of IPC message. See below.

*-v, --version*
	Print the version (of swaymsg) and quit.

# IPC MESSAGE TYPES

*<command>*
	The message is a sway command (the same commands you can bind to keybindings
	in your sway config file). It will be executed immediately.

	See **sway**(5) for a list of commands.

*get\_workspaces*
	Gets a JSON-encoded list of workspaces and their status.

*get\_inputs*
	Gets a JSON-encoded list of current inputs.

*get\_outputs*
	Gets a JSON-encoded list of current outputs.

*get\_tree*
	Gets a JSON-encoded layout tree of all open windows, containers, outputs,
	workspaces, and so on.

*get\_marks*
	Get a JSON-encoded list of marks.

*get\_bar\_config*
	Get a JSON-encoded configuration for swaybar.

*get\_version*
	Get JSON-encoded version information for the running instance of sway.

*get\_binding\_modes*
	Gets a JSON-encoded list of currently configured binding modes.

*get\_config*
	Gets a JSON-encoded copy of the current configuration.

*get\_clipboard*
	Get JSON-encoded information about the clipboard.
	Returns the current clipboard mime-types if called without
	arguments, otherwise returns the clipboard data in the requested
	formats. Encodes the data using base64 for non-text mime types.
