sway-save-tree(1)

# NAME

sway-save-tree - Dump a workspace's tiling tree as append_layout-compatible JSON.

# SYNOPSIS

_sway-save-tree_ --workspace <name>

# DESCRIPTION

Reads the live tree of the running sway instance over the IPC socket, walks
the named workspace, prunes runtime fields, and emits a JSON document
suitable for *swaymsg append_layout*(5). Each leaf view becomes a
placeholder entry whose *swallows* array contains a regex-anchored match on
the view's *app_id* (Wayland) or *class* / *instance* (xwayland).

The output is sent to standard output. Pre-existing placeholders that were
loaded by *append_layout* and have not yet swallowed a window are echoed
verbatim from their *swallows* array.

This is the sway counterpart of i3-save-tree(1).

# OPTIONS

*--workspace* <name>
	Name (or number, as a string) of the workspace to dump. Required.

*-h, --help*
	Show help and exit.

# LIMITATIONS

Floating windows are skipped in this release; only tiling children of the
workspace are emitted. A warning is printed to stderr when floating windows
are present.

# EXAMPLES

Save the layout of workspace 1 to a file, kill the running windows, and
restore the layout:

```
$ sway-save-tree --workspace 1 > /tmp/layout.json
$ swaymsg [workspace=1] kill
$ swaymsg append_layout /tmp/layout.json
```

# SEE ALSO

*sway*(5), *sway*(1), *swaymsg*(1)
