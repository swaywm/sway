# sway

[**English**](https://github.com/swaywm/sway/blob/master/README.md#sway--) - [日本語](https://github.com/swaywm/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/swaywm/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/swaywm/sway/blob/master/README.el.md#sway--) - [Français](https://github.com/swaywm/sway/blob/master/README.fr.md#sway--) - [Українська](https://github.com/swaywm/sway/blob/master/README.uk.md#sway--) - [Italiano](https://github.com/swaywm/sway/blob/master/README.it.md#sway--) - [Português](https://github.com/swaywm/sway/blob/master/README.pt.md#sway--) -
[Русский](https://github.com/swaywm/sway/blob/master/README.ru.md#sway--)

"**S**irCmpwn's **Way**land compositor" is a **work in progress**
i3-compatible [Wayland](http://wayland.freedesktop.org/) compositor.
Read the [FAQ](https://github.com/swaywm/sway/wiki). Join the
[IRC channel](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

**Notice**: You are viewing the **unstable** and **unsupported** master branch
of sway, where work is ongoing to port it to
[wlroots](https://github.com/swaywm/wlroots). The supported branch is the 0.15
branch, and end users are encouraged to use the stable releases cut from it.

If you'd like to support sway development, please contribute to [SirCmpwn's
Patreon page](https://patreon.com/sircmpwn).

## Release Signatures

Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/swaywm/sway/releases).

## Status

- [i3 feature support](https://github.com/swaywm/sway/issues/2)
- [IPC feature support](https://github.com/swaywm/sway/issues/98)
- [i3bar feature support](https://github.com/swaywm/sway/issues/343)
- [i3-gaps feature support](https://github.com/swaywm/sway/issues/307)
- [security features](https://github.com/swaywm/sway/issues/984)

## Installation

### From Packages

Sway is available in many distributions. Try installing the "sway" package for
yours. If it's not available, check out [this wiki page](https://github.com/swaywm/sway/wiki/Unsupported-packages)
for information on installation for your distributions.

If you're interested in packaging sway for your distribution, stop by the IRC
channel or shoot an email to sir@cmpwn.com for advice.

### Compiling from Source

Install dependencies:

* meson
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* pcre
* json-c >= 0.13
* pango
* cairo
* gdk-pixbuf2 *
* pam **
* dbus >= 1.10 ***
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (required for man pages)

_\*Only required for swaybar, swaybg, and swaylock_

_\*\*Only required for swaylock_

_\*\*\*Only required for tray support_

Run these commands:

    meson build
    ninja -C build
    sudo ninja -C build install

On systems with logind, you need to set a few caps on the binary:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/local/bin/sway

On systems without logind, you need to suid the sway binary:

    sudo chmod a+s /usr/local/bin/sway

## Configuration

If you already use i3, then copy your i3 config to `~/.config/sway/config` and
it'll work out of the box. Otherwise, copy the sample configuration file to
`~/.config/sway/config`. It is usually located at `/etc/sway/config`.
Run `man 5 sway` for information on the configuration.

## Running

Run `sway` from a TTY. Some display managers may work but are not supported by
sway (gdm is known to work fairly well).
