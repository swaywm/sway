#!/usr/bin/python

# This script requires i3ipc-python package (install it from a system package manager
# or pip).
# It makes inactive windows transparent. Use `transparency_val` variable to control
# transparency strength in range of 0…1.

import i3ipc

transparency_val = '0.8';
ipc              = i3ipc.Connection()
prev_focused     = None

for window in ipc.get_tree():
    if window.focused:
        prev_focused = window
    else:
        window.command('opacity ' + transparency_val)

def on_window_focus(ipc, focused):
    global prev_focused
    if focused.container.id != prev_focused.id: # https://github.com/swaywm/sway/issues/2859
        focused.container.command('opacity 1')
        prev_focused.command('opacity ' + transparency_val)
        prev_focused = focused.container

ipc.on("window::focus", on_window_focus)
ipc.main()
