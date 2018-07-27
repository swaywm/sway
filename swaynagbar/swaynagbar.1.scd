swaynagbar(1)

# NAME

swaynagbar - Show a warning or error message with buttons

# SYNOPSIS

_swaynagbar_ [options...]

# OPTIONS
*-b, --button* <text> <action>
	Create a button with the text _text_ that executes _action_ when pressed.
	Multiple buttons can be defined by providing the flag multiple times.

*-d, --debug*
	Enable debugging.

*-e, --edge top|bottom*
	Set the edge to use.

*-f, --font <font>*
	Set the font to use.

*-h, --help*
	Show help message and quit.

*-l, --detailed-message <msg>*
	Set the detailed message. A button to toggle details will be added. Details
	are shown in a scrollable multi-line text area. If _msg_ is _-_, then the
	detailed message will be read from stdin.

*-L, --detailed-button <text>*
	Set the text for the button that toggles details. This has no effect if
	there is not a detailed message. The default is _Toggle Details_.

*-m, --message <msg>*
	Set the message text.

*-o, --output <output>*
	Set the output to use. This should be the name of a _xdg\_output_. If
	_xdg\_output\_manager_ is not supported, then the first detected output
	will be used

*-s, --dismiss-button <text>*
	Sets the text for the dismiss nagbar button. The default is _X_.

*-t, --type error|warning*
	Set the message type.

*-v, --version
	Show the version number and quit.

