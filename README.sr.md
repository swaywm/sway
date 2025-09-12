# sway

sway је [i3]-компатибилан [Wayland] композитор. Прочитајте [FAQ]. Придружите се
[IRC каналу] \(#sway на irc.libera.chat).

## Потписи Издања

Издања су потписана са [E88F5E48] и објављена [на GitHub-у][GitHub releases].

## Инсталација

### Из пакета

Sway је доступан у многим дистрибуцијама. Покушајте да инсталирате "sway" пакет за
вашу.

### Компајлирање из Извора

Погледајте [ову вики страницу][Development setup], ако желите да компајлирате HEAD верзију
sway-а и wlroots-а за тестирање или развој.

Инсталирајте зависности:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (опционо: додатни формати слика за системску траку)
* [swaybg] (опционо: позадина)
* [scdoc] (опционо: man странице) \*
* git (опционо: информације о верзији) \*

_\* Потребно само за компајлирање_

Покрените следеће команде:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## Конфигурација

Ако већ користите i3, копирајте вашу i3 конфигурацију у `~/.config/sway/config` и
радиће одмах. У супротном, копирајте пример конфигурационе датотеке у
`~/.config/sway/config`. Обично се налази у `/etc/sway/config`.
Покрените `man 5 sway` за информације о конфигурацији.

## Покретање

Покрените `sway` из TTY-a или из менаџера приказа.

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC каналу]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
