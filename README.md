# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor" is a **work in progress**
i3-compatible [Wayland](http://wayland.freedesktop.org/) compositor.
Read the [FAQ](https://github.com/SirCmpwn/sway/wiki). Join the
[IRC channel](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

[More screenshots](https://github.com/SirCmpwn/sway/wiki/Screenshots-of-Sway)

## Release Signatures

Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/SirCmpwn/sway/releases).

## Status

- [i3 feature support](https://github.com/SirCmpwn/sway/issues/2)
- [IPC feature support](https://github.com/SirCmpwn/sway/issues/98)
- [i3bar feature support](https://github.com/SirCmpwn/sway/issues/343)
- [i3-gaps feature support](https://github.com/SirCmpwn/sway/issues/307)
- [security features](https://github.com/SirCmpwn/sway/issues/984)

[Bounties](https://github.com/SirCmpwn/sway/issues/986): sponsor features or get paid to write them

## Installation

### From Packages

* [Arch Linux](https://github.com/SirCmpwn/sway/wiki/Install-from-packages#arch)
* [Fedora](https://github.com/SirCmpwn/sway/wiki/Install-from-packages#fedora)
* [Gentoo](https://github.com/SirCmpwn/sway/wiki/Install-from-packages#gentoo)
* [NixOS](https://github.com/SirCmpwn/sway/wiki/Install-from-packages#nixos)
* [openSUSE Tumbleweed](https://github.com/SirCmpwn/sway/wiki/Install-from-packages#opensuse)

For other distros, [see this wiki page](https://github.com/SirCmpwn/sway/wiki/Install-from-packages#unofficial-packages).
If you're interested in packaging Sway for your distribution, stop by the IRC
channel or shoot an email to sir@cmpwn.com for advice.

### Compiling from Source

Install dependencies:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
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
    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway

On systems without logind, you need to suid the sway binary:

    sudo chmod a+s /usr/local/bin/sway

## Configuration

If you already use i3, then copy your i3 config to `~/.config/sway/config` and
it'll work out of the box. Otherwise, copy the sample configuration file to
`~/.config/sway/config`. It is usually located at `/etc/sway/config`.
Run `man 5 sway` for information on the configuration.

My own dotfiles are available [here](https://gogs.sr.ht/SirCmpwn/dotfiles) if
you want some inspiration, and definitely check out the
[wiki](https://github.com/SirCmpwn/sway/wiki) as well.

## Running

Instead of running `startx`, run `sway`. You can run `sway` from within X as
well, which is useful for testing.
