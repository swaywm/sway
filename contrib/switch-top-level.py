#!/usr/bin/env python3
import i3ipc
#
# This script requires i3ipc-python package (install it from a system package manager
# or pip).
#
# The scripts allows you to define two new bindings:
#    bindsym $mod+bracketright nop top_next
#    bindsym $mod+bracketleft nop top_prev
#
# The purpose of it is to switch between top-level containers (windows) in a workspace.
# One possible usecase is having a workspace with two (or more on large displays)
# columns of tabs: one on left and one on right. In such setup, "move left" and
# "move right" will only switch tabs inside the column.
#
# You can add a systemd user service to run this script on startup:
#
# ~> cat .config/systemd/user/switch-top-level.service
# [Install]
# WantedBy=graphical-session.target

# [Service]
# ExecStart=path/to/switch-top-level.py
# Restart=on-failure
# RestartSec=1

# [Unit]
# Requires=graphical-session.target


class TopLevelSwitcher:
    def __init__(self):
        self.top_to_selected = {} # top i3ipc.Con -> selected container id
        self.con_to_top = {} # container id -> top i3ipc.Con
        self.prev = None # previously focused container id

        self.i3 = i3ipc.Connection()
        self.i3.on('window::focus', self.on_window_focus)
        self.i3.on(i3ipc.Event.BINDING, self.on_binding)

        self.update_top_level()
        self.i3.main()

    def top_level(self, node):
        if len(node.nodes) == 1:
            return self.top_level(node.nodes[0])
        return node.nodes

    def update_top_level(self):
        tree = self.i3.get_tree()
        for ws in tree.workspaces():
            for con in self.top_level(ws):
                self.update_top_level_rec(con, con.id)

    def update_top_level_rec(self, con: i3ipc.Con, top: i3ipc.Con):
        self.con_to_top[con.id] = top
        for child in con.nodes:
            self.update_top_level_rec(child, top)

        if len(con.nodes) == 0 and top not in self.top_to_selected:
            self.top_to_selected[top] = con.id

    def save_prev(self):
        if not self.prev:
            return
        prev_top = self.con_to_top.get(self.prev)
        if not prev_top:
            return
        self.top_to_selected[prev_top] = self.prev


    def on_window_focus(self, _i3, event):
        self.update_top_level()
        self.save_prev()
        self.prev = event.container.id

    def on_top(self, _i3, _event, diff: int):
        root = self.i3.get_tree()
        if not self.prev:
            return
        top = self.con_to_top[self.prev]
        ws = [top.id for top in self.top_level(root.find_focused().workspace())]

        top_idx = ws.index(top)
        top_idx = (top_idx + diff + len(ws)) % len(ws)
        next_top = ws[top_idx]
        next_window = self.top_to_selected.get(next_top)
        self.i3.command('[con_id=%s] focus' % next_window)

    def on_binding(self, i3, event):
        if event.binding.command.startswith('nop top_next'):
            self.on_top(i3, event, 1)
        elif event.binding.command.startswith('nop top_prev'):
            self.on_top(i3, event, -1)


if __name__ == '__main__':
    TopLevelSwitcher()
