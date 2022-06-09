# swayFX: A Beautiful Sway Fork

Sway is an incredible window manager, and certainly one of the most well established wayland window managers. However, it is restricted to only include the functionality that existed in i3. This fork ditches the simple wlr_renderer, and replaces it with a fancy GLES2 renderer with functionality borrowed from the original simple renderer and [Hyprland](https://github.com/vaxerski/Hyprland). This, along with a couple of minor changes, expands sway's featureset to include the following:

+ **Scratchpad treated as minimize**: Allows docks, or panels with a taskbar, to correctly interpret minimize / unminimize requests ([thanks to LCBCrion](https://github.com/swaywm/sway/issues/6457))
+ **Default to not compiling swaybar**: Many users replace swaybar with the far more capable [waybar](https://github.com/Alexays/Waybar), swayFX cuts out the bloat by not including swaybar by default
+ **Add a nix flake to the repo**: Allows nixos users to easily contribute to and test this project

## Roadmap:
+ fade in / out animations
+ window movement animations
+ drop shadows
+ blur
+ rounded corners
