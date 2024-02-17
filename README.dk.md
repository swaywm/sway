# sway

[English][en] - [Česky][cs] - [Deutsch][de] - **[Dansk][dk]** - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Sway er en [i3]-kompatibel [Wayland] compositor. Læs [Ofte stillede spørgsmål][FAQ].
Deltag på [IRC kanalen][IRC channel] \(#sway på irc.libera.chat).

## Udgivelses Signaturer

Udgivelser er signeret med [E88F5E48] og publiceret [på GitHub][GitHub
releases].

## Installation

### Fra pakker

Sway er tilgængelig i mange distributioner. Prøv at installere "sway" pakken
fra din.

Hvis du er interesseret i at pakke Sway til din distribution, kan du tage forbi
IRC kanalen eller sende en email til sir@cmpwn.com for rådgivning.

### Kompilering fra kildekode

Se [denne wiki-side][Development setup] hvis du vil bygge HEAD af sway og
wlroots til test eller udvikling.

Installationsafhængigheder:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (valgfrit: system tray)
* [scdoc] (valgfrit: man pages) \*
* git \*

_\*Kompileringsafhængighed_

Kør følgende kommandoer:

    meson build
    ninja -C build
    sudo ninja -C build install

## Konfiguration

Hvis du allerede bruger i3 kan du bare kopiere din i3 konfiguration til
`~/.config/sway/config`. Ellers skal du kopiere eksempelkonfigurationsfilen til
`~/.config/sway/config`. Den er normalt placeret i `/etc/sway/config`.  Kør
`man 5 sway` for at få oplysninger om konfigurationen.

## Eksekvering

Kør `sway` fra en TTY. Nogle display managers kan fungere, men Sway yder ikke
support til dem (gdm er kendt for at fungere temmelig godt).

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
