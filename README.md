# swaypower: A Beautiful Sway Fork

Sway is an incredible window manager, and certainly one of if the the most well established wayland window managers. However, it is restricted in its feature set to include only what i3 included. This fork expands sway's featureset to include the following:

+ **[Scratchpad treated as minimize](https://github.com/WillPower3309/swaypower/commit/f6aac41efee81c3edfda14be8ddb375827c81d9e)**: Allows docks, or panels with a taskbar, to correctly interpret minimize / unminimize requests ([thanks to LCBCrion](https://github.com/swaywm/sway/issues/6457))
+ **Default to not compiling swaybar**: Many users replace swaybar with the far more capable [waybar](https://github.com/Alexays/Waybar), this repo cuts out the bloat by not including swaybar by default
+ **Add a nix flake to the repo**: Allows nixos users to easily contribute to and test this repo


## Roadmap:
+ fade in / out animations
+ window movement animations
+ drop shadows
+ blur
+ rounded corners
