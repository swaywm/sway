sway-bar(5)

# NAME

sway-bar - bar configuration file and commands

# DESCRIPTION

Sway allows configuring swaybar in the sway configuration file. Swaybar
commands must be used inside a _bar { }_ block in the config file.

# COMMANDS

*status\_command* <status command>
	Executes the bar _status command_ with _sh -c_. Each line of text printed
	to stdout from this command will be displayed in the status area of the
	bar. You may also use the i3bar JSON protocol:

	https://i3wm.org/docs/i3bar-protocol.html

*pango\_markup* enabled|disabled
	Enables or disables pango markup for status lines. This has no effect on
	status lines using the i3bar JSON protocol.

*id* <bar\_id>
	Sets the ID of the bar.

*position* top|bottom
	Sets position of the bar. Default is _bottom_.

*output* <output>
	Restrict the bar to a certain output, can be specified multiple times. If
	the output command is omitted, the bar will be displayed on all outputs.

*swaybar\_command* <command>
	Executes custom bar command. Default is _swaybar_.

*font* <font>
	Specifies the font to be used in the bar.

*separator\_symbol* <symbol>
	Specifies the separator symbol to separate blocks on the bar.

*wrap\_scroll* yes|no
	Enables or disables wrapping when scrolling through workspaces with the
	scroll wheel. Default is _no_.

*workspace\_buttons* yes|no
	Enables or disables workspace buttons on the bar. Default is _yes_.

*strip\_workspace\_numbers* yes|no
	If set to _yes_, then workspace numbers will be omitted from the workspace
	button and only the custom name will be shown. Default is _no_.

*binding\_mode\_indicator* yes|no
	Enable or disable binding mode indicator. Default is _yes_.

*height* <height>
	Sets the height of the bar. Default height will match the font size.

## TRAY

Swaybar provides a system tray where third-party applications may place icons.
The following commands configure the tray.

The _button_ argument in all cases is a platform-specific button code. On Linux
you can find a list of these at linux/input-event-codes.h.

*activate\_button* <button>
	Sets the button to be used for the _activate_ (primary click) tray item
	event. The default is BTN\_LEFT (0x110).

*context\_button* <button>
	Sets the button to be used for the _context menu_ (right click) tray item
	event. The default is BTN\_RIGHT (0x111).

*secondary\_button* <button>
	Sets the button to be used for the _secondary_ (middle click) tray item
	event. The default is BTN\_MIDDLE (0x112).

*tray\_output* none|all|<output>
	Sets the output that the tray will appear on or none. Unlike i3bar, swaybar
	is able to show icons on any number of bars and outputs without races.
	The default is _all_.

*tray\_padding* <px> [px]
	Sets the pixel padding of the system tray. This padding will surround the
	tray on all sides and between each item. The default value for _px_ is 2.

*icon\_theme* <name>
	Sets the icon theme that sway will look for item icons in. This option has
	no default value, because sway will always default to the fallback theme,
	hicolor.

## COLORS

Colors are defined within a _colors { }_ block inside a _bar { }_ block. Colors
must be defined in hex: _#RRGGBB_ or _#RRGGBBAA_.

*background* <color>
	Background color of the bar.

*statusline* <color>
	Text color to be used for the statusline.

*separator* <color>
	Text color to be used for the separator.

*focused\_background* <color>
	Background color of the bar on the currently focused monitor output. If not
	used, the color will be taken from _background_.

*focused\_statusline* <color>
	Text color to be used for the statusline on the currently focused monitor
	output. If not used, the color will be taken from _statusline_.

*focused\_separator* <color>
	Text color to be used for the separator on the currently focused monitor
	output. If not used, the color will be taken from _separator_.

*focused\_workspace* <border> <background> <text>
	Border, background and text color for a workspace button when the workspace
	has focus.

*active\_workspace* <border> <background> <text>
	Border, background and text color for a workspace button when the workspace
	is active (visible) on some output, but the focus is on another one. You
	can only tell this apart from the focused workspace when you are using
	multiple monitors.

*inactive\_workspace* <border> <background> <text>
	Border, background and text color for a workspace button when the workspace
	does not have focus and is not active (visible) on any output. This will be
	the case for most workspaces.

*urgent\_workspace* <border> <background> <text>
	Border, background and text color for a workspace button when the workspace
	contains a window with the urgency hint set.

*binding\_mode* <border> <background> <text>
	Border, background and text color for the binding mode indicator. If not used,
	the colors will be taken from _urgent\_workspace_.

# SEE ALSO

*sway*(5)

