#!/usr/bin/python

# This script requires i3ipc-python package (install it from a system package manager or pip).
# Use with --help flag for usage isntructions.

import argparse
import i3ipc
import signal
import sys
import re
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
        elif prev_focused.app_id in args.inactive_overrides.keys():
            prev_focused.command("opacity " + args.inactive_overrides[prev_focused.app_id])
        else:
            prev_focused.command("opacity " + args.inactive_opacity)

        if focused.app_id in args.ignore:
            focused.command("opacity 1")
        elif focused.app_id in args.active_overrides.keys():
            focused.command("opacity " + args.active_overrides[focused.app_id])
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
        "--inactive-overrides",
        "-I",
        type=str,
        default=[],
        help="List of appliations with their values that override the inactive opactity settings. (Example: -A firefox=0.9 kitty=0.8)",
        nargs="+"
    )
    parser.add_argument(
        "--active-overrides",
        "-A",
        type=str,
        default=[],
        help="List of appliations with their values that override the active opactity settings. (Example: -A firefox=0.9 kitty=0.8)",
        nargs="+"
    )
    parser.add_argument(
        "--ignore",
        type=str,
        default=[],
        help="List of applications to be ignored.",
        nargs="+"
    )
    args = parser.parse_args()

    # Convert ovveride arguments of format app_id=opacity_value
    # to dictionary of format "app_id": "opacity_value"
    temp_inactive_overrides_dictionary = {}
    for override in args.inactive_overrides:
        app_id = re.search('\S*(?==)', override).group(0)
        opacity_value = re.search('(?<==)\S*', override).group(0)
        temp_inactive_overrides_dictionary[app_id] = opacity_value
    args.inactive_overrides = temp_inactive_overrides_dictionary

    temp_active_overrides_dictionary = {}
    for override in args.active_overrides:
        app_id = re.search('\S*(?==)', override).group(0)
        opacity_value = re.search('(?<==)\S*', override).group(0)
        temp_active_overrides_dictionary[app_id] = opacity_value
    args.active_overrides = temp_active_overrides_dictionary

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
