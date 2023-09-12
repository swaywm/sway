# sway

[English][en] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - **[Svenska][sv]** - [Ελληνικά][gr] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway är en [i3]-kompatibel [Wayland] compositor. Läs våran [FAQ]-sida. Gå med i vår
[IRC-kanal] \(#sway på irc.libera.chat).

## Utgåvosignaturer

Utgåvor är signerade med [E88F5E48] och publicerade på [GitHub][GitHub releases].

## Installering

### Med pakethanterare

Sway är tillgänglig i många distributioner. Prova att installera "sway" med din distributions pakethanterare.

### Genom att kompilera från källkod

Kolla in [denna wiki-sida][Development setup] om du vill bygga sway och wlroots HEAD för testning eller utveckling.

Installera paket som sway behöver:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (valbar: systembricka)
* [scdoc] (valbar: manualer) \*
* git (valbar: versioninfo) \*

_\* Krav för kompilering_

Kör dessa kommandon:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## Konfiguration

Ifall du redan använder i3 så kan du kopiera din konfigurationsfil till `~/.config/sway/config` och det kommer då att fungera som det ska.
Kopiera annars exemplarkonfigurationsfilen till `~/.config/sway/config`. Den ligger oftast i `/etc/sway/config`.
Kör `man 5 sway` för mer information kring konfigurationen.

## Att köra sway

Kör `sway` från en TTY. Vissa inloggningahanterare kan fungera men inte vara stödda av sway (gdm ska fungera hyfsat bra).

[en]: https://github.com/swaywm/sway#readme
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[sv]: README.sv.md
[gr]: README.gr.md
[hu]: README.hu.md
[ir]: README.ir.md
[it]: README.it.md
[ja]: README.ja.md
[ko]: README.ko.md
[nl]: README.nl.md
[pl]: README.pl.md
[pt]: README.pt.md
[ro]: README.ro.md
[ru]: README.ru.md
[tr]: README.tr.md
[uk]: README.uk.md
[zh-CN]: README.zh-CN.md
[zh-TW]: README.zh-TW.md
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC-kanal]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
