# sway

"**S**irCmpwn's **Way**land window manager"

sway is an i3-compatible window manager for
[Wayland](http://wayland.freedesktop.org/).

![](https://sr.ht/iQyr.png)

## Rationale

I use i3 on xorg. Wayland is coming, and [i3way](http://i3way.org/) still has
zero lines of source code after two years.

## Status

[See "i3 feature support"](https://github.com/SirCmpwn/sway/issues/2)

## Compile / Install

Dependencies:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* xkb

Notably missing: weston is not required.

    cmake .
    make
    # sudo make install

Binary shows up in `./bin` (or `/usr/bin` if you `make install`).

## Configuration

    mkdir ~/.sway
    cp ~/.i3/config ~/.sway/config

[See also](http://i3wm.org/docs/)

## Running

    sway

If you run this while xorg is running, it'll run inside of an x window (useful
for testing). Otherwise, it'll run wayland properly.
