swaynag(5)

# NAME

swaynag - swaynag configuration file

# SYNOPSIS

$HOME/.swaynag/config, $XDG\_CONFIG\_HOME/swaynag/config,
SYSCONFDIR/swaynag/config

# CONFIG FILE

At the top of the config file, _swaynag_ options can be set using the format
_long-option=value_. These will be used as default values if _swaynag_ is not
given the option. This can be useful for setting a preferred font, output, and
edge.

Below the options, custom types may be defined. To define a type, use the
following format:

```
[name-of-type]
option=value
```

All colors may be given in the form _RRGGBB_ or _RRGGBBAA_. The following
colors can be set:

*background=<color>*
	The background color for _swaynag_.

*border=<color>*
	The color to use for borders of buttons.

*border-bottom=<color>*
	The color of the border line at the bottom of _swaynag_.

*button-background=<color>*
	The background color for the buttons.

*text=<color>*
	The color of the text.

*button-text=<color>*
	The color of the button text.

The following sizing options can also be set:

*border-bottom-size=<size>*
	Set the thickness of the bottom border.

*message-padding=<padding>*
	Set the padding for the message.

*details-background=<color>*
	The background color for the details.

*details-border-size=<size>*
	Set the thickness for the details border.

*button-border-size=<size>*
	Set the thickness for the button border.

*button-gap=<gap>*
	Set the size of the gap between buttons.

*button-dismiss-gap=<gap>*
	Set the size of the gap between the dismiss button and another button.

*button-margin-right=<margin>*
	Set the margin from the right of the dismiss button to edge.

*button-padding=<padding>*
	Set the padding for the button text.

Additionally, the following options can be assigned a default per-type:

*edge=top|bottom*
	Set the edge to use.

*layer=overlay|top|bottom|background*
	Set the layer to use.

*font=<font>*
	Set the font to use.

*output=<output>*
	Set the output to use. This should be the name of a _xdg\_output_.

# EXAMPLE

```
font=Monospace 12
edge=bottom

[green]
edge=top
background=00AA00
border=006600
border-bottom=004400
text=FFFFFF
button-background=00CC00
message-padding=10
```

# SEE

swaynag(1)
