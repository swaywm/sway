swayrun(1)

# NAME

swayrun - Run Sway from a login shell

# SYNOPSIS

_swayrun_ [options...] [command]

# DESCRIPTION

Attempts to run *sway*(1) via the user's login shell. This allows environment
configuration and other login actions, for example when run via a login manager.

All arguments to swayrun will be passed through unaltered to sway.

If a valid login shell cannot be determined, sway will be executed normally.

# CONFIGURATION

swayrun will consult */etc/shells* if it exists for a list of valid login
shells. In addition, *false* and *nologin* will be considered invalid shells.

# ENVIRONMENT

The following environment variables have an effect on swayrun:

_SHELL_
	Holds the path to the login shell.

# SEE

*sway*(1) *shells*(5)
