swayidle (1)

# NAME

swayidle - Idle manager for Wayland

# SYNOPSIS

*swayidle* [options] [events...]

# OPTIONS

*-h*
	Show help message and quit.

*-d*
	Enable debug output.

# DESCRIPTION

swayidle listens for idle activity on your Wayland compositor and executes tasks
on various idle-related events. You can specify any number of events at the
command line.

# EVENTS

*timeout* <timeout> <timeout command> [resume <resume command>]
	Execute _timeout command_ if there is no activity for <timeout> seconds.
	
	If you specify "resume <resume command>", _resume command_ will be run when
	there is activity again.

*before-sleep* <command>
	If built with systemd support, executes _command_ before systemd puts the
	computer to sleep.

All commands are executed in a shell.

# EXAMPLE

```
 swayidle \
     timeout 300 'swaylock -c 000000' \
     timeout 600 'swaymsg "output * dpms off"' \
		resume 'swaymsg "output * dpms on"' \
     before-sleep 'swaylock -c 000000'
```

This will lock your screen after 300 seconds of inactivity, then turn off your
displays after another 600 seconds, and turn your screens back on when resumed.
It will also lock your screen before your computer goes to sleep.

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about sway development, see
https://github.com/swaywm/sway.

# SEE ALSO

*sway*(5) *swaymsg*(1) *swaygrab*(1) *sway-input*(5) *sway-bar*(5)
