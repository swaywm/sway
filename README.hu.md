# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - **[Magyar][hu]** - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

A Sway egy [i3]-kompatibilis [Wayland] kompozitor. Olvasd el a [Gyarkan Ismételt Kérdéseket][FAQ]. Csatlakozz az [IRC csatornához][IRC channel] \(`#sway` az `irc.libera.chat`-en).

## Csomag aláírások

A kiadott csomagok az [E88F5E48] kulccsal vannak aláírva és [GitHub-on][GitHub releases] publikálva.

## Telepítés

### Csomagból

A Sway sok disztribúció csomagkezelőjéből elérhető, próbáld meg a "sway"
csomagot telepíteni az általad használt eszközzel.

Ha szeretnél csomagot készíteni a saját disztribúciódhoz, ugorj be az IRC
csatornára, vagy küldj levelet a sir@cmpwn.com címre tanácsokért.

### Fordítás forráskódból

Olvasd el [ezt a wiki oldalt][Development setup], ha szeretnéd tesztelési vagy
fejlesztési célokból lefordítani az aktuális (HEAD) állapotát a `sway`-nek és a
`wlroots`-nak.

Telepítsd a függőségeket:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (opcionális: system tray)
* [scdoc] (opcionális: man pages) \*
* git (opcionális: version info) \*

_\*Fordításidejű függőség_

Futtasd ezeket a parancsokat:

    meson build
    ninja -C build
    sudo ninja -C build install

## Konfiguráció

Ha előzőleg i3-mat használtál, akkor átmásolhatod az i3 beállításaidat a
`~/.config/sway/config` file-ba és ugyanúgy működni fognak. Egyéb esetben másold
le kiindulási alapnak a mintát, ami általában az `etc/sway/config` elérési
útvonalon található.
Futtasd a `man 5 sway` parancsot további információért a konfigurációval
kapcsolatban.

## Futtatás

Futtasd a `sway` parancsot egy TTY felületről. Néhány bejelentkezéskezelő
(display manager) működhet, de alapvetően nem támogatottak a sway által. (A
gdm-ről ismeretes, hogy egész jól működik.)

[en]: https://github.com/swaywm/sway#readme
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[gr]: README.gr.md
[hi]: README.hi.md
[hu]: README.hu.md
[ir]: README.ir.md
[it]: README.it.md
[ja]: README.ja.md
[ko]: README.ko.md
[nl]: README.nl.md
[no]: README.no.md
[pl]: README.pl.md
[pt]: README.pt.md
[ro]: README.ro.md
[ru]: README.ru.md
[sv]: README.sv.md
[tr]: README.tr.md
[uk]: README.uk.md
[zh-CN]: README.zh-CN.md
[zh-TW]: README.zh-TW.md
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
