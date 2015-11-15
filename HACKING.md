## Code overview

The following is a brief code overview / general introduction for those wanting
to hack on sway.

### wlc

In Wayland the compositor is the display server. That's a design decision that
brings several advantages, but the downside is that all compositors need to
implement an entire display server as well.

To aid the situation there are several *wayland display servers* being
implemented as libraries so that compositors can stick to doing compositing and
leave the low level details to one of those libraries. In sway that library is
`wlc`, and it handles tty switching, logind sessions, input, everything that
deals with the GPU, and just about everything concerning the Wayland protocol
itself (as of writing there's not a single call to any wayland functions inside
of sway). sway communicates with wlc via a callback api found in
`sway/handlers` (`wlc_interface`). The code in that file deals with all the
entry points from wlc to sway.

### Commands

Being a tiling window manager, controlling it via commands is an important part
of its functionality, and `sway/commands` which deals with that is by far the
biggest file in the codebase.

There are multiple ways to trigger a command: via the keyboard, via the config
file, or via the IPC interface.

### IPC

i3 has an IPC interface (it creates a socket that applications can connect to
and issue commands or queries via its protocol), and sway replicates that
protocol (so e.g. `i3-msg` can be used with sway by simply changing the socket,
e.g. `i3-msg -s $(sway --get-socketpath)`). The code for that lies in
`sway/ipc`.

### Config

The config state and loading the config file lies in `sway/config`. Since the
config file is simply a list of commands, that code mostly just parses the text
and then hands commands off to `commands` for execution.

### Pointer handling

The mouse has buttons, state (due to buttons pressed, e.g. "dragging",
"resizing" etc.) and movement. Most code related to that lies in
`sway/input_state`.

### Containers

In traditional *floating* window managers, all windows (or *views* as they're
called in sway) are placed anywhere on the screen. In a tiling window manager
like sway the views are *arranged* by the compositor, and the user mostly just
manipulates the arrangement via commands (floating views are also supported).

In sway, each *output* (a physical monitor) has one or more *workspaces* which
has one or more *views* (the actual windows). In order to keep track of the
arrangement of the views, sway organizes everything in a tree of *containers*.
Each of the previously mentioned things is a type of container. In addition
there's a type of container called *container* which is needed to arrange other
containers as siblings (horizontal or vertical layout), and a *root container*
which exists for practical reasons.

`sway/containers` contains the code for this and understanding containers is
essential in understanding sway.

Also, the code that actually arranges the different views lays in
`sway/layout`.

### Focus

When changing workspace, changing output, changing view or just moving the
pointer you change which view has *focus*. The code for handling this and
e.g. deciding what view receives input events is handled in `sway/focus`.

### Notes

As sway is a work in progress, as of writing it is still not versioned. Use the
`master` branch of sway and wlc for now.
