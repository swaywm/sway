# sway-borders

sway-borders is a fork of [Sway](https://swaywm.org), an [i3](https://i3wm.org/)-compatible [Wayland](http://wayland.freedesktop.org/) compositor. It introduces some new features like more customizable borders, but is otherwise kept up to date with [sway](https://github.com/swaywm/sway).

Please refer to the [Sway GitHub](https://github.com/swaywm/sway/) for docs and related material which isn't related to the new features below.

## Installation
The following package distributions exist. If you package sway-borders for another distribution, feel free to PR its entry here.
|Distribution|Name|Maintainer|
|---|---|---|
|AUR|`sway-borders-git`|TheAvidDev|

Releases will follow Sway's. To compile from source, follow the same procedure as Sway [here](https://github.com/swaywm/sway#compiling-from-source).

# Features
 - [X] Border images (allow for drop shadows, outer curved borders, etc.)
 - [ ] Curved borders (inner)
Descriptions and usage of the features can be found below. If you would like some more features that won't be added by Sway, feel free to request them for this project instead.

## Border Images
This feature allows the use of eight images that get snapped on to the corners and edges of windows. This allows for any combination of outer curved borders, drop shadows, and multi-layer borders. These border images are drawn wherever gaps normally appear.

### Configuration
Directly from the manpage:
```
*border-images.<class>* <folder_path>
	Configures the images used for borders. The _folder_path_ is expected to be
	the full path, with a trailing slash, to a folder that contains 8 PNG images
	named 0.png, 1.png, ..., 7.png. These images are used in clockwise order,
	starting from the top-left corner, ending on the left edge. For the classes
  below, _container_ refers to a container which has gaps around it.

  The available classes are:

	*border_images.focused*
		The container which is focused or has a window that has focus.

	*border_images.focused_inactive*
		The container which has the most recently focused view within a container
		which is not focused.

	*border_images.unfocused*
		A container with all of its views unfocused.

	*border_images.urgent*
		A container which has view with an urgency hint. *Note*: Native Wayland windows do not
		support urgency. Urgency only works for Xwayland windows.
```

Unlike pixel borders, the border images will overflow into gaps, so you may have to alter your gaps to accomidate.

To use this in your config, you would probably use the following:
```
exec_always border-images.focused /some/folder
exec_always border-images.focused_inactive /some/folder
exec_always border-images.unfocused /some/folder
exec_always border-images.urgent /some/folder
```

Note: A folder which doesn't contain any images, or only contains some of the images will not throw any errors or warnings, do double check your folder path if you experience issues.

### Samples
The [`/contrib/borders/` folder](https://github.com/TheAvidDev/sway-borders/tree/master/contrib/borders/) contains some example and community contributed border images, alongside screenshots. Feel free to add your own and make a PR!

## Rounded Borders
While the border images allow for rounded borders to be added on the _outside_ of containers, always increasing the total size of the container. An option to round the inner borders, cropping the container content itself is planned.
