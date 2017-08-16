# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

[**English**](https://github.com/SirCmpwn/sway/blob/master/README.md#sway--) - [日本語](https://github.com/SirCmpwn/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/SirCmpwn/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/SirCmpwn/sway/blob/master/README.el.md#sway--) - [Français](https://github.com/SirCmpwn/sway/blob/master/README.fr.md#sway--) - [Español](https://github.com/SirCmpwn/sway/blob/master/README.es.md#sway--) - [Українська](https://github.com/SirCmpwn/sway/blob/master/README.uk.md#sway--) - [Italian](https://github.com/SirCmpwn/sway/blob/master/README.it.md#sway--)


"**S**irCmpwn's **Way**land compositor" is a **work in progress**
i3-compatible [Wayland](http://wayland.freedesktop.org/) compositor.
Read the [FAQ](https://github.com/SirCmpwn/sway/wiki). Join the
[IRC channel](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

If you'd like to support Sway development, you can contribute to [SirCmpwn's
Patreon page](https://patreon.com/sircmpwn) or you can contribute to
[bounties](https://github.com/SirCmpwn/sway/issues/986) for specific features.
Anyone is welcome to claim a bounty and you can make a bounty for any feature
you wish, and Patreon is more useful for supporting the overall health and
maintenance of Sway.

## Release Signatures

Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/SirCmpwn/sway/releases).

## Status

- [i3 feature support](https://github.com/SirCmpwn/sway/issues/2)
- [IPC feature support](https://github.com/SirCmpwn/sway/issues/98)
- [i3bar feature support](https://github.com/SirCmpwn/sway/issues/343)
- [i3-gaps feature support](https://github.com/SirCmpwn/sway/issues/307)
- [security features](https://github.com/SirCmpwn/sway/issues/984)

## Installation

### From Packages

Sway is available in many distributions. Try installing the "sway" package for
yours. If it's not available, check out [this wiki page](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages)
for information on installation for your distributions.

If you're interested in packaging Sway for your distribution, stop by the IRC
channel or shoot an email to sir@cmpwn.com for advice.

### Compiling from Source

Install dependencies:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* asciidoc
* pcre
* json-c
* pango
* cairo
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

On systems with logind, you need to set a few caps on the binary:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

On systems without logind, you need to suid the sway binary:

    sudo chmod a+s /usr/local/bin/sway

## Configuration

If you already use i3, then copy your i3 config to `~/.config/sway/config` and
it'll work out of the box. Otherwise, copy the sample configuration file to
`~/.config/sway/config`. It is usually located at `/etc/sway/config`.
Run `man 5 sway` for information on the configuration.

## Running

Run `sway` from a TTY. Some display managers may work but are not supported by
Sway (gdm is known to work fairly well).
