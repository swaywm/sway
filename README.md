# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land window manager" is a **work in progress**
i3-compatible window manager for [Wayland](http://wayland.freedesktop.org/).
Read the [FAQ](https://github.com/SirCmpwn/sway/wiki). Join the
[IRC channel](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

![](https://sr.ht/me1j.png)

## Release Signatures

Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/SirCmpwn/sway/releases).

## Status

- [i3 feature support](https://github.com/SirCmpwn/sway/issues/2)
- [IPC feature support](https://github.com/SirCmpwn/sway/issues/98)
- [i3bar feature support](https://github.com/SirCmpwn/sway/issues/343)
- [i3-gaps feature support](https://github.com/SirCmpwn/sway/issues/307)

## Installation

### From Packages

Sway is not supported by many distributions yet. Here's a list of packages
available for you to install:

* [Arch Linux](https://www.archlinux.org/packages/?sort=&q=sway&maintainer=&flagged=)

For other distros, [see this wiki page](https://github.com/SirCmpwn/sway/wiki/Install-on-other-distros).
If you're interested in packaging Sway for your distribution, stop by the IRC
channel or shoot an email to sir@cmpwn.com for advice.

### Compiling from Source

Install dependencies:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* asciidoc
* pcre
* json-c
* pango *
* cairo *
* gdk-pixbuf2 *
* pam **
* imagemagick (required for image capture with swaygrab)
* ffmpeg (required for video capture with swaygrab)

_\*Only required for swaybar, swaybg, and swaylock_

_\*\*Only required for swaylock_

Run these commands:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

On systems without logind, you need to suid the sway binary:

    sudo chmod a+s /usr/local/bin/sway

## Configuration

If you already use i3, then copy your i3 config to `~/.config/sway/config` and
it'll work out of the box. Otherwise, copy the sample configuration file to
`~/.config/sway/config`. It is usually located at `/etc/sway/config`.
Run `man 5 sway` for information on the configuration.

## Running

Instead of running `startx`, run `sway`. You can run `sway` from within X as
well, which is useful for testing.
