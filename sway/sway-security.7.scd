sway-security(7)

# NAME

sway-security - Guidelines for securing your sway install

# SECURITY OVERVIEW

*Sway is not considered secure*. We are working on it but do not trust that we
have it all figured out yet. The following man page is provisional.

Securing sway requires careful configuration of your environment, the sort
that's usually best suited to a distribution maintainer who wants to ship a
secure sway environment in their distribution. Sway provides a number of means
of securing it but you must make a few changes external to sway first.

Configuration of security features is limited to files in the security
directory (this is likely _/etc/sway/security.d/\*_, but depends on your
installation prefix).  Files in this directory must be owned by _root:root_ and
chmod _644_ or _444_. The default security configuration is installed to
_/etc/sway/security.d/00-defaults_, and should not be modified - it will be
updated with the latest recommended security defaults between releases. To
override the defaults, you should add more files to this directory.

Package maintainers who ship software which needs extra permissions for sway
should include a file in this directory for that purpose.

# ENVIRONMENT SECURITY

*LD\_PRELOAD* is a mechanism designed to ruin the security of your system.
There are a number of strategies for dealing with this, but they all suck a
little. In order of most practical to least practical:

. Only run important programs via exec. Sway's exec command will ensure that
  *LD\_PRELOAD* is unset when running programs.
. Remove *LD\_PRELOAD* support from your dynamic loader (requires patching
  libc). This may break programs that rely on *LD\_PRELOAD* for legitimate
  functionality, but this is the most effective solution.
. Use static linking for important programs. Of course statically linked
  programs are unaffected by the dynamic linking security dumpster fire.

Note that should you choose method 1, you MUST ensure that sway itself isn't
compromised by *LD\_PRELOAD*. It probably isn't, but you can be sure by setting
_/usr/bin/sway_ to a+s (setuid), which will instruct the dynamic linker not to
permit *LD\_PRELOAD* for it (and will also run it as root, which sway will
shortly drop). You could also statically link sway itself.

Note that *LD\_LIBRARY\_PATH* has all of these problems, and the same
solutions.

# IPC SECURITY

Clients which have access to the IPC socket can use any IPC feature they want.
Ensure untrusted clients do not have access to the IPC socket.

# FEATURE POLICIES

Certain sway features are security sensitive and may be configured with
security policies. These features are:

*fullscreen*
	Permission to become fullscreen. Note that users can always make a window
	fullscreen themselves with the fullscreen command.

Additional features can be controlled by the name of their Wayland global.

By default, no permissions are granted (though saner defaults are provided in
_/etc/sway/config.d/security_). You can use the following configuration options
to control a program's access:

*permit* <executable> <features...>
	Permits _executable_ to use _features_ (each feature separated by a space).
	_executable_ may be \* to affect the default policy, or the full path to
	the executable file.

*reject* <executable> <features...>
	Disallows _executable_ from using _features_ (each feature separated by a
	space). _executable_ may be \* to affect the default policy, or the full
	path to the executable file.

By default, the following Wayland globals are hidden by default unless a
*permit* statement is issued for them:

*zwlr\_data\_control\_manager\_v1*
	Used to monitor all clipboard activity.

*zwlr\_export\_dmabuf\_manager\_v1*, *zwlr\_screencopy\_manager\_v1*
	Both of these protocols are used to capture images of your screen.

*zwlr\_gamma\_control\_manager\_v1*
	Used to control gamma settings, i.e. Redshift functionality.

*zwlr\_input\_inhibit\_manager\_v1*
	Used to obtain exclusive input access, by lock screens and the like.

*zwlr\_layer\_shell\_v1*
	Used for panels, wallpapers, notifications, and other desktop components.

*zwp\_virtual\_keyboard\_manager\_v1*
	Used by on-screen keyboards.

*IMPORTANT*: Sway is only able to enforce the security policy for clients which
are spawned by sway via the *exec* or *exec\_always* sway commands. You can use
*swaymsg(1)* to run the *exec* command externally. Any commands not executed in
this manner are given the default policy.

When you first declare a policy for an executable, it will inherit the default
policy. Further changes to the default policy will not retroactively affect
which permissions an earlier policy inherits. You must explicitly reject any
features from the default policy that you do not want an executable to receive
permission for.

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about sway development, see
https://github.com/swaywm/sway.
