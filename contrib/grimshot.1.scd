grimshot(1)

# NAME

grimshot - a helper for screenshots within sway

# DESCRIPTION

grimshot [--notify] (copy|save) [TARGET] [FILE]
grimshot check
grimshot usage

*--notify*
	Show notifications to the user that a screenshot has been taken.

*save*
	Save the screenshot into a regular file. Grimshot will write images
	files to *$XDG_SCREENSHOTS_DIR* if this is set (or defined
	in `user-dirs.dir`), or otherwise fall back to `$XDG_PICTURES_DIR`

*copy*
	Copy the screenshot data (as image/png) into the clipboard.

# SYNOPSIS

Grimshot is an easy to use screenshot tool for sway. It relies on grim, slurp
and jq to do the heavy lifting, and mostly provides an easy to use interface.

A recommended usage pattern is to just bind this to Super+P via sway:

```
# Screenshots:
# Cmd+P: Current window
# Cmd+Shift+p: Select area
# Cmd+Alt+p Current output
# Cmd+Ctrl+p Select a window

bindsym Mod4+p       exec grimshot save active
bindsym Mod4+Shift+p exec grimshot save area
bindsym Mod4+Mod1+p  exec grimshot save output
bindsym Mod4+Ctrl+p  exec grimshot save window
```

# TARGETS

grimshot can capture the following named targets:

_active_
	Captures the currently active window.

_screen_
	Captures the entire screen. This includes all visible outputs.

_area_
	Allows manually selecting a rectangular region, and captures that.

_window_
	Allows manually selecting a single window (by clicking on it), and
	captures it.

_output_
	Captures the currently active output.

# OUTPUT

Grimshot will always print the filename of the captured screenshot to
stdout.

# SEE ALSO

*grim*(1)
