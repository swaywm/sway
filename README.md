# SwayFX: A Beautiful Sway Fork

![](assets/swayfx_screenshot.jpg)

Sway is an incredible window manager, and certainly one of the most well established wayland window managers. However, it is restricted to only include the functionality that existed in i3. This fork ditches the simple wlr_renderer, and extends it to render fancy GLES2 effects. This, along with a couple of minor changes, expands sway's featureset to include the following:

+ **Anti-aliased rounded corners, borders, and titlebars**
+ **Scratchpad treated as minimize**: Allows docks, or panels with a taskbar, to correctly interpret minimize / unminimize requests ([thanks to LCBCrion](https://github.com/swaywm/sway/issues/6457))
+ **Add a nix flake to the repo**: Allows nixos users to easily contribute to and test this project

## Configuration
+ Corner radius: `corner_radius <val>`

## Roadmap:
+ fade in / out animations
+ window movement animations
+ drop shadows
+ blur
