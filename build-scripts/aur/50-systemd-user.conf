# sway does not set DISPLAY/WAYLAND_DISPLAY in the systemd user environment
# See FS#63021
# Adapted from xorg's 50-systemd-user.sh, which achieves a similar goal.

exec systemctl --user import-environment DISPLAY WAYLAND_DISPLAY SWAYSOCK XDG_CURRENT_DESKTOP
exec hash dbus-update-activation-environment 2>/dev/null && \
     dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY SWAYSOCK XDG_CURRENT_DESKTOP
