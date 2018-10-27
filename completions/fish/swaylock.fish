# swaylock(1) completion

complete -c swaylock -s C -l config --description 'The config file to use. Default: $HOME/.swaylock/config, $XDG_CONFIG_HOME/swaylock/config, and SYSCONFDIR/swaylock/config.'
complete -c swaylock -s h -l help --description "Show help message and quit."
complete -c swaylock -s f -l daemonize --description "Fork into the background after spawning. Note: this is the default bahavior of i3lock."
complete -c swaylock -s v -l version --description "Show the version number and quit."
complete -c swaylock -s s -l socket --description "Use the specified socket path. Otherwise, swaymsg will as sway where the socket is (which is the value of $SWAYSOCK, then of $I350CK)."
complete -c swaylock -s e -l ignore-empty-password --description 'When an empty password is provided by the user, do not validate it.'

# Appearance
complete -c swaylock -s u -l no-unlock-indicator --description "Disable the unlock indicator."
complete -c swaylock -s i -l image --description "Display the given image, optionally on the given output. Use -c to set a background color."
complete -c swaylock -s s -l scaling --description "Scaling mode for images: stretch, fill, fit, center, or tile."
complete -c swaylock -s t -l tiling --description "Same as --scaling=tile."
complete -c swaylock -s c -l color --description "Turn the screen into the given color. If -i is used, this sets the background of the image into the given color. Defaults to white (ffffff), or transparent (00000000) if an image is in use."
complete -c swaylock -l bs-hl-color --description 'Sets the color of backspace highlight segments.'
complete -c swaylock -l font --description 'Sets the font of the text inside the indicator.'
complete -c swaylock -l indicator-radius --description 'Sets the radius of the indicator to radius pixels. Default: 50'
complete -c swaylock -l indicator-thickness --description 'Sets the thickness of the indicator to thickness pixels. Default: 10'
complete -c swaylock -l inside-color --description 'Sets the color of the inside of the indicator when typing or idle.'
complete -c swaylock -l inside-clear-color --description 'Sets the color of the inside of the indicator when cleared.'
complete -c swaylock -l inside-ver-color --description 'Sets the color of the inside of the indicator when verifying.'
complete -c swaylock -l inside-wrong-color --description 'Sets the color of the inside of the indicator when invalid.'
complete -c swaylock -l key-hl-color --description 'Sets the color of key press highlight segments.'
complete -c swaylock -l line-color --description 'Sets the color of the lines that separate the inside and outside of the indicator when typing or idle.'
complete -c swaylock -l line-clear-color --description 'Sets the color of the lines that separate the inside and outside of the indicator when cleared.'
complete -c swaylock -l line-ver-color --description 'Sets the color of the lines that separate the inside and outside of the indicator when verifying.'
complete -c swaylock -l line-wrong-color --description 'Sets the color of the lines that separate the inside and outside of the indicator when invalid.'
complete -c swaylock -s n -l line-uses-inside --description 'Use the color of the inside of the indicator for the line separating the inside and outside of the indicator.'
complete -c swaylock -s r -l line-uses-ring --description 'Use the outer ring\'s color for the line separating the inside and outside of the indicator.'
complete -c swaylock -l ring-color --description 'Sets the color of the outside of the indicator when typing or idle.'
complete -c swaylock -l ring-clear-color --description 'Sets the color of the outside of the indicator when cleared.'
complete -c swaylock -l ring-ver-color --description 'Sets the color of the outside of the indicator when verifying.'
complete -c swaylock -l ring-wrong-color --description 'Sets the color of the outside of the indicator when invalid.'
complete -c swaylock -l separator-color --description 'Sets the color of the lines that separate highlight segments.'
complete -c swaylock -l text-color --description 'Sets the color of the text inside the indicator when typing or idle.'
complete -c swaylock -l text-clear-color --description 'Sets the color of the text inside the indicator when cleared.'
complete -c swaylock -l text-ver-color --description 'Sets the color of the text inside the indicator when verifying.'
complete -c swaylock -l text-wrong-color --description 'Sets the color of the text inside the indicator when invalid.'
