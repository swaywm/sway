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
color=RRGGBB[AA]
```

All colors may be given in the form _RRGGBB_ or _RRGGBBAA_. The following
colors can be set:

*background*
	The background color for _swaynag_.

*border*
	The color to use for borders of buttons.

*border-bottom*
	The color of the border line at the bottom of _swaynag_.

*button-background*
	The background color for the buttons.

*text*
	The color of the text.

# EXAMPLE
```
font=Monospace 12

[green]
background=00AA00
border=006600
border-bottom=004400
text=FFFFFF
button-background=00CC00
```

# SEE
swaynag(1)
