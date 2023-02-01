swaymsg(1)

# NAME

swaymsg - Send messages to a running instance of sway over the IPC socket.

# SYNOPSIS

_swaymsg_ [options...] [message]

# OPTIONS

*-h, --help*
	Show help message and quit.

*-m, --monitor*
	Monitor for responses until killed instead of exiting after the first
	response. This can only be used with the IPC message type _subscribe_. If
	there is a malformed response or an invalid event type was requested,
	swaymsg will stop monitoring and exit.

*-p, --pretty*
	Use pretty output even when not using a tty.
	Not available for all message types.

*-q, --quiet*
	Sends the IPC message but does not print the response from sway.

*-r, --raw*
	Use raw JSON output even if using a tty.

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

	See *sway*(5) for a list of commands.

	_swaymsg_ can return pretty printed (standalone-default) or JSON-formatted
	(*--raw*) output. For detailed documentation on the returned JSON-data of
	each message type listed below,	refer to *sway-ipc*(7). The JSON-format can
	contain more information than the pretty print.

	Tips:
	- Command expansion is performed twice: once by swaymsg, and again by sway.
	  If you have quoted multi-word strings in your command, enclose the entire
	  command in single-quotes. For example, use
	  _swaymsg 'output "Foobar Display" enable'_ instead of
	  _swaymsg output "Foobar Display" enable_. Furthermore, note that comma
	  separated options also count as multi-word strings, because commas can be
	  used to execute commands on the same line.
	- If you are providing a command that contains a leading hyphen (_-_), insert
	  two hyphens (_--_) before the command to signal to swaymsg not to parse
	  anything beyond that point as an option. For example, use
	  _swaymsg -- mark --add test_ instead of _swaymsg mark --add test_.

*get\_workspaces*
	Gets a list of workspaces and their status.

*get\_inputs*
	Gets a list of current inputs.

*get\_outputs*
	Gets a list of current outputs.

*get\_tree*
	Gets a JSON-encoded layout tree of all open windows, containers, outputs,
	workspaces, and so on.

*get\_seats*
	Gets a list of all seats,
	its properties and all assigned devices.

*get\_marks*
	Get a JSON-encoded list of marks.

*get\_bar\_config*
	Get a JSON-encoded configuration for swaybar.

*get\_version*
	Get version information for the running instance of sway.

*get\_binding\_modes*
	Gets a JSON-encoded list of currently configured binding modes.

*get\_binding\_state*
	Gets JSON-encoded info about the current binding state.

*get\_config*
	Gets a copy of the current configuration. Doesn't expand includes.

*send\_tick*
	Sends a tick event to all subscribed clients.

*subscribe*
	Subscribe to a list of event types. The argument for this type should be
	provided in the form of a valid JSON array. If any of the types are invalid
	or if a valid JSON array is not provided, this will result in a failure.
	For a list of valid event types and the data returned with them refer to
	*sway-ipc*(7).

# RETURN CODES

*0*
	Success

*1*
	swaymsg errors such as invalid syntax, unable to connect to the ipc socket
	or unable to parse sway's reply

*2*
	Sway returned an error when processing the command (ex. invalid command,
	command failed, and invalid subscription request)

# SEE ALSO

*sway*(5) *sway-bar*(5) *sway-input*(5) *sway-output*(5) *sway-ipc*(7)
