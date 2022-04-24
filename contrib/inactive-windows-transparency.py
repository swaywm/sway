#!/usr/bin/python

# This script requires i3ipc-python package (install it from a system package manager
# or pip).
# It makes inactive windows transparent. Use `transparency_val` variable to control
# transparency strength in range of 0…1 or use the command line argument -o.

import argparse
import i3ipc
import json
import signal
import sys
from functools import partial


class Opts:
    def __init__(self, args):
        self.ignore = args.ignore
        self.active_opacities = json.loads(args.active_opacities)
        self.inactive_opacity = args.opacity


def set_focused_opacity(window, opts):
    opacity = opts.active_opacities.get(window.app_id, 1)
    cmd = "opacity {}".format(opacity)
    window.command(cmd)
    return


def on_window_focus(opts, ipc, event):
    global prev_focused
    global prev_workspace

    focused_workspace = ipc.get_tree().find_focused()

    if focused_workspace == None:
        return

    focused = event.container
    workspace = focused_workspace.workspace().num

    if focused.id != prev_focused.id:  # https://github.com/swaywm/sway/issues/2859
        set_focused_opacity(focused, opts);
        if workspace == prev_workspace:
            if not window.app_id in opts.ignore:
                prev_focused.command("opacity " + opts.inactive_opacity)
                prev_focused = focused
                prev_workspace = workspace


def remove_opacity(ipc):
    for workspace in ipc.get_tree().workspaces():
        for w in workspace:
            w.command("opacity 1")
            ipc.main_quit()
            sys.exit(0)


if __name__ == "__main__":
    transparency_val = "0.80"

    parser = argparse.ArgumentParser(
        description="This script allows you to set the transparency of unfocused windows in sway."
    )
    parser.add_argument(
        "--opacity",
        "-o",
        type=str,
        default=transparency_val,
        help="set opacity value in range 0...1",
    )
    parser.add_argument(
        "--active-opacities",
        "-a",
        type=str,
        default="{}",
        help="A dictionary of applications and their active opacities."
    )
    parser.add_argument(
        "--ignore",
        "-i",
        type=str,
        default=[],
        help="List of ignored processes.",
        nargs="+"
    )
    args = parser.parse_args()
    opts = Opts(args)

    ipc = i3ipc.Connection()
    prev_focused = None
    prev_workspace = ipc.get_tree().find_focused().workspace().num

    for window in ipc.get_tree():
        if window.focused:
            prev_focused = window
        elif not window.app_id in opts.ignore:
            window.command("opacity " + args.opacity)
    for sig in [signal.SIGINT, signal.SIGTERM]:
        signal.signal(sig, lambda signal, frame: remove_opacity(ipc))
        ipc.on("window::focus", partial(on_window_focus, opts))
        ipc.main()
