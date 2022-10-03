#!/usr/bin/python

# This script requires i3ipc-python package (install it from a system package manager or pip).
# Use with --help flag for usage isntructions.

import argparse
import i3ipc
import signal
import sys
from functools import partial

def on_window_focus(args, ipc, event):
    global prev_focused

    focused_workspace = ipc.get_tree().find_focused()

    if focused_workspace == None:
        return

    focused = event.container


    # on_window_focus not called only when focused is changed,
    # but also when a window is moved
    if focused.id != prev_focused.id:
        if prev_focused.app_id in args.ignore:
            prev_focused.command("opacity 1")
        else:
            prev_focused.command("opacity " + args.inactive_opacity)

        if focused.app_id in args.ignore:
            focused.command("opacity 1")
        else: 
            focused.command("opacity " + args.active_opacity)
        prev_focused = focused


def remove_opacity(ipc):
    tree = ipc.get_tree()
    for workspace in tree.workspaces():
        for window in workspace:
            window.command("opacity 1")
    for window in tree.scratchpad():
        window.command("opacity 1")
    ipc.main_quit()
    sys.exit(0)


if __name__ == "__main__":
    default_inactive_opacity = "0.80"
    default_active_opacity = "1.0"

    parser = argparse.ArgumentParser(
        description="This script allows you to set the transparency of focused and unfocused windows in sway."
    )
    parser.add_argument(
        "--inactive-opacity",
        "-i",
        type=str,
        default=default_inactive_opacity,
        help="value between 0 and 1 denoting opacity for inactive windows",
    )
    parser.add_argument(
        "--active-opacity",
        "-a",
        type=str,
        default=default_active_opacity,
        help="value between 0 and 1 denoting opacity for active windows",
    )
    parser.add_argument(
        "--ignore",
        type=str,
        default=[],
        help="List of applications to be ignored.",
        nargs="+"
    )
    args = parser.parse_args()

    ipc = i3ipc.Connection()
    prev_focused = None

    for window in ipc.get_tree():
        if window.focused:
            prev_focused = window
        else:
            window.command("opacity " + args.inactive_opacity)
    for sig in [signal.SIGINT, signal.SIGTERM]:
        signal.signal(sig, lambda signal, frame: remove_opacity(ipc))
    ipc.on("window::focus", partial(on_window_focus, args))
    ipc.main()
