# sway

"**S**irCmpwn's **Way**land window manager"

sway is a **work in progress** i3-compatible window manager for
[Wayland](http://wayland.freedesktop.org/).

![](https://sr.ht/qxGE.png)

Chat on #sway on irc.freenode.net

## Rationale

I use i3 on xorg. Wayland is coming, and [i3way](http://i3way.org/) still has
zero lines of source code after two years.

## Status

[See "i3 feature support"](https://github.com/SirCmpwn/sway/issues/2)

## Installation

### Arch Linux

Install [aur/sway-git](https://aur.archlinux.org/packages/sway-git/).

### Manual

Dependencies:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* xwayland

Compiling:

    cmake .
    make
    # sudo make install

Binary shows up in `./bin` (or `/usr/local/bin` if you `make install`).

## Configuration

    mkdir ~/.config/sway
    cp ~/.config/i3/config ~/.config/sway/

Or if you don't already use i3:

    mkdir ~/.config/sway
    cp /etc/sway/config ~/.config/sway/

Edit to your liking.

[See also](http://i3wm.org/docs/)

## Running

    sway

If you run this while xorg is running, it'll run inside of an x window (useful
for testing). Otherwise, it'll run wayland properly.
