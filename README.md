# sway-borders

sway-borders is a fork of [Sway](https://swaywm.org), an [i3](https://i3wm.org/)-compatible [Wayland](http://wayland.freedesktop.org/) compositor. It introduces some new features like more customizable borders, but is otherwise kept up to date with [sway](https://github.com/swaywm/sway).

Please refer to the [Sway GitHub](https://github.com/swaywm/sway/) for docs and related material which isn't related to the new features below.

## Installation
The following package distributions exist. If you package sway-borders for another distribution, feel free to PR its entry here.
|Distribution|Name|Maintainer|Notes|
|---|---|---|---|
|AUR|`sway-borders-git`|TheAvidDev|Not created yet, but will be shortly.|

Releases will follow Sway's.

To compile from source, follow the same procedure as Sway [here](https://github.com/swaywm/sway#compiling-from-source).

# Features
 - [X] Border images (allow for drop shadows, outer curved borders, etc.)
 - [ ] Curved borders (inner)
 - [ ] Blur
 - And more, see [sway#3380](https://github.com/swaywm/sway/issues/3380)
 
Descriptions and usage of the features can be found below. If you would like some more features that won't be added by Sway, feel free to request them for this project instead.

## Border Images
This feature allows the use of eight images that get snapped on to the corners and edges of windows. This allows for any combination of outer curved borders, drop shadows, and multi-layer borders. These border images are drawn wherever gaps normally appear.

### Configuration
Directly from the manpage:
```
*border_images.<class>* <path>
	Configures the images used for borders. The _path_ is expected to be an
	absolute path to an image with an odd width and height which will be scaled to
	container sizes. The edges are expected to be 1 pixel in width for top and
	bottom edges, and 1 pixel in height for left edges as they will be stretched
	across container edges.

	For the classes below, "container" refers to a container which has gaps
	around it. The available classes are:
	
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

In less technical terms, you can draw your borders around a 1x1 pixel in the center of your image. This image doesn't have to be a square, but for offsets across a single axis, you have to use completely transparent pixels since the center of the image will always be used.

Unlike pixel borders, the border images will overflow into gaps, so you may have to alter your gaps to accomidate.

To use this in your config, you would probably use the following:
```
border_images.focused /some/folder/
border_images.focused_inactive /some/folder/
border_images.unfocused /some/folder/
border_images.urgent /some/folder/
```

The [`/contrib/borders/` folder](https://github.com/TheAvidDev/sway-borders/tree/master/contrib/borders/) contains some example and community contributed border images, alongside screenshots. Feel free to add your own and make a PR!

## Rounded Borders
While the border images allow for rounded borders to be added on the _outside_ of containers, always increasing the total size of the container. An option to round the inner borders, cropping the container content itself is planned.

## Blur
It would be nice to add bluring of semi-transparent windows since it's hard to use them with more complex backgrounds. This has quite a few nuances and may even require a custom wlroots build, we'll see.

See: [sway#4356](https://github.com/swaywm/sway/issues/4356)
