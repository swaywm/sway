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
        self.active_opacity = args.active_opacity
        self.active_opacities = json.loads(args.active_opacities)
        self.inactive_opacity = args.inactive_opacity
        self.inactive_opacities = json.loads(args.inactive_opacities)
        self.ignore = args.ignore


def set_opacity(window, opts, active=True):
    app = window.app_id
    opacity = None
    if app in opts.ignore:
        opacity = 1.0
    else:
        if active:
            opacity = opts.active_opacities.get(app, opts.active_opacity)
        else:
            opacity = opts.inactive_opacities.get(app, opts.inactive_opacity)
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
        set_opacity(focused, opts, active=True);
        if workspace == prev_workspace:
            set_opacity(prev_focused, opts, active=False)
            prev_focused = focused
            prev_workspace = workspace


def remove_opacity(ipc):
    for workspace in ipc.get_tree().workspaces():
        for w in workspace:
            w.command("opacity 1")
            ipc.main_quit()
            sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="This script allows you to set the transparency of windows in sway."
    )
    parser.add_argument(
        "--active_opacity",
        "-a",
        type=str,
        default=1,
        help="The default opacity for active windows.",
    )
    parser.add_argument(
        "--active-opacities",
        "-A",
        type=str,
        default="{}",
        help="A dictionary of applications and their active opacities."
    )
    parser.add_argument(
        "--inactive_opacity",
        "-i",
        type=str,
        default=0.8,
        help="The default opacity for inactive windows.",
    )
    parser.add_argument(
        "--inactive_opacities",
        "-I",
        type=str,
        default="{}",
        help="A dictionary of applications and their inactive opacities."
    )
    parser.add_argument(
        "--ignore",
        type=str,
        default=[],
        help="List of applications to be ignored.",
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
        set_opacity(window, opts, active=False)
    for sig in [signal.SIGINT, signal.SIGTERM]:
        signal.signal(sig, lambda signal, frame: remove_opacity(ipc))
    ipc.on("window::focus", partial(on_window_focus, opts))
    ipc.main()
