# swaylock(1) completion

complete -c swaylock -s h -l help --description "Show help message and quit."
complete -c swaylock -s c -l color --description "Turn the screen into the given color. If -i is used, this sets the background of the image into the given color. Defaults to white (ffffff), or transparent (00000000) if an image is in use."
complete -c swaylock -s f -l daemonize --description "Fork into the background after spawning. Note: this is the default bahavior of i3lock."
complete -c swaylock -s i -l image --description "Display the given image, optionally on the given output. Use -c to set a background color."
complete -c swaylock -l scaling --description "Scaling mode for images: stretch, fill, fit, center, or tile."
complete -c swaylock -s t -l tiling --description "Same as --scaling=tile."
complete -c swaylock -s u -l no-unlock-indicator --description "Disable the unlock indicator."
complete -c swaylock -s v -l version --description "Show the version number and quit."
complete -c swaylock -l socket --description "Use the specified socket path. Othherwise, swaymsg will as sway where the socket is (which is the value of $SWAYSOCK, then of $I350CK)."
