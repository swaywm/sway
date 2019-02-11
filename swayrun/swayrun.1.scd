swayrun(1)

# NAME

swayrun - Run Sway from a login shell

# SYNOPSIS

_swayrun_ [options...] [command]

# DESCRIPTION

Attempts to run *sway*(1) via the user's login shell. This allows environment
configuration and other login actions, for example when run via a display manager.

All arguments to swayrun will be passed through unaltered to sway.

If a login shell cannot be determined, sway will be executed normally.

# ENVIRONMENT

The following environment variables have an effect on swayrun:

_SHELL_
	Holds the path to the login shell.

# SEE

*sway*(1)
