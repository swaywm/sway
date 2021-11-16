swaybar-protocol(7)

# NAME

swaybar-protocol - JSON status line protocol for swaybar

# SUMMARY

swaybar defines the following JSON protocol to specify what information is
displayed in the status line on the right side of swaybar. The protocol
comprises a header, that is a JSON object, followed by a newline (*0x0A*),
followed by an infinite JSON array that represents the information to display.
All communication is done by writing the status line to standard output and
reading events from standard input.

# HEADER

The header is a JSON object with support for the following properties (only
_version_ is required):
[- *PROPERTY*
:- *DATA TYPE*
:- *DEFAULT*
:- *DESCRIPTION*
|- version
:  integer
:  -
:[ The protocol version to use. Currently, this must be *1*
|- click_events
:  boolean
:  false
:  Whether to receive click event information to standard input
|- cont_signal
:  integer
:  SIGCONT
:  The signal that swaybar should send to continue processing
|- stop_signal
:  integer
:  SIGSTOP
:  The signal that swaybar should send to stop processing

## MINIMAL HEADER
```
{
	"version": 1
}
```

## FULL HEADER
```
{
	"version": 1,
	"click_events": true,
	"cont_signal": 18,
	"stop_signal": 19
}
```

# BODY

The body is an infinite array, where each element of the array is a
representation of the status line at the time that the element was written.
Each element of the array is itself an array of JSON objects, where each
object represents a block in the status line. Each block can have the following
properties (only _full_text_ is required):

[- *PROPERTY*
:- *DATA TYPE*
:- *DESCRIPTION*
|- full_text
:  string
:[ The text that will be displayed. *If missing, the block will be skipped.*
|- short_text
:  string
:  If given and the text needs to be shortened due to space, this will be
   displayed instead of _full_text_
|- color
:  string
:  The text color to use in #RRGGBBAA or #RRGGBB notation
|- background
:  string
:  The background color for the block in #RRGGBBAA or #RRGGBB notation
|- border
:  string
:  The border color for the block in #RRGGBBAA or #RRGGBB notation
|- border_top
:  integer
:  The height in pixels of the top border. The default is 1
|- border_bottom
:  integer
:  The height in pixels of the bottom border. The default is 1
|- border_left
:  integer
:  The width in pixels of the left border. The default is 1
|- border_right
:  integer
:  The width in pixels of the right border. The default is 1
|- min_width
:  integer or string
:  The minimum width to use for the block. This can either be given in pixels
   or a string can be given to allow for it to be calculated based on the
   width of the string.
|- align
:  string
:  If the text does not span the full width of the block, this specifies how
   the text should be aligned inside of the block. This can be _left_
   (default), _right_, or _center_.
|- name
:  string
:  A name for the block. This is only used to identify the block for click
   events. If set, each block should have a unique _name_ and _instance_ pair.
|- instance
:  string
:  The instance of the _name_ for the block. This is only used to identify the
   block for click events. If set, each block should have a unique _name_ and
   _instance_ pair.
|- urgent
:  boolean
:  Whether the block should be displayed as urgent. Currently swaybar utilizes
   the colors set in the sway config for urgent workspace buttons. See
   *sway-bar*(5) for more information on bar color configuration.
|- separator
:  boolean
:  Whether the bar separator should be drawn after the block. See *sway-bar*(5)
   for more information on how to set the separator text.
|- separator_block_width
:  integer
:  The amount of pixels to leave blank after the block. The separator text will
   be displayed centered in this gap. The default is 9 pixels.
|- markup
:  string
:  The type of markup to use when parsing the text for the block. This can
   either be _pango_ or _none_ (default).

Other properties may be given and swaybar will ignore any property that it does
not know. It is recommended to prefix any custom properties with an underscore
to make it easier to distinguish them from protocol properties.

## BODY EXAMPLE

```
[
	[
		{
			"full_text": "25%",
			"min_width": "100%",
			"urgent": false
		},
		{
			"full_text": "Thu 30 May 2019 02:15:15"
		}
	],
	[
		{
			"full_text": "20%",
			"min_width": "100%",
			"urgent": false
		},
		{
			"full_text": "Thu 30 May 2019 02:20:52"
		}
	],
	[
		{
			"full_text": "15%",
			"min_width": "100%",
			"urgent": true
		},
		{
			"full_text": "Thu 30 May 2019 02:25:41"
		}
	],
	...
```

## FULL BLOCK EXAMPLE
```
{
	"full_text": "Thu 30 May 2019 02:09:15",
	"short_text": "02:09",
	"color": "#ccccccff",
	"background": "#111111ff",
	"border": "#222222ff",
	"border_top": 1,
	"border_bottom": 1,
	"border_left": 1,
	"border_right": 1,
	"min_width": 100,
	"align": "center",
	"name": "clock",
	"instance": "edt",
	"urgent": false,
	"separator": true,
	"separator_block_width": 5,
	"markup": "none"
}
```

# CLICK EVENTS

If requested in the header, swaybar will write a JSON object, that can be read
from standard in, when the user clicks on a block. The event object will have
the following properties:

[- *PROPERTY*
:- *DATA TYPE*
:- *DESCRIPTION*
|- name
:  string
:[ The name of the block, if set
|- instance
:  string
:  The instance of the block, if set
|- x
:  integer
:  The x location that the click occurred at
|- y
:  integer
:  The y location that the click occurred at
|- button
:  integer
:  The x11 button number for the click. If the button does not have an x11
   button mapping, this will be _0_.
|- event
:  integer
:  The event code that corresponds to the button for the click
|- relative_x
:  integer
:  The x location of the click relative to the top-left of the block
|- relative_y
:  integer
:  The y location of the click relative to the top-left of the block
|- width
:  integer
:  The width of the block in pixels
|- height
:  integer
:  The height of the block in pixels

*Differences from i3bar's protocol:*
- _button_ may be _0_ for buttons that do not have x11 button mappings
- _event_ has been introduced to allow for non-x11 buttons to be represented
- The _modifiers_ property is not currently added by swaybar

## EXAMPLE

```
{
	"name": "clock",
	"instance": "edt",
	"x": 1900,
	"y": 10,
	"button": 1,
	"event": 274,
	"relative_x": 100,
	"relative_y": 8,
	"width": 120,
	"height": 18
}
```

# SEE ALSO

*sway-bar*(5)
