# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - **[Norsk][no]** - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Sway er en [i3]-kompatibel [Wayland] compositor. Les [Ofte stilte spørsmål].
Delta på [IRC kanalen][IRC channel] \(#sway på irc.libera.chat).

## Utgivelses Signaturer

Utgivelser er signert med [E88F5E48] og publisert [på GitHub][GitHub
releases].

## Installasjon

### Fra system pakker

Sway er tilgjengelig i mange distribusjoner. Prøv å installere "sway" pakken
fra din distro sine repoer.

Er du interessert i å pakke Sway for din distribusjon kan du ta turen innom
IRC-kanalen eller send en e-post til sir@cmpwn.com for råd.

### Kompilering fra kildekode

Se [denne wiki-siden][Development setup] hvis du vil bygge fra HEAD grenen av sway og
wlroots for testing eller utvikling.

Installasjonsavhengigheter:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (valgfritt: system tray)
* [scdoc] (valgfritt: man pages) \*
* git \*

_\*Kompileringsavhengigheter_

Kjør følgende kommandoer:

    meson build
    ninja -C build
    sudo ninja -C build install

På systemer uten logind eller seatd skal du sette SUID bit i sway filen:

    sudo chmod a+s /usr/local/bin/sway

Sway slipper 'root' tillatelser kort etter oppstart.

## Konfigurasjon

Hvis du allerede bruker i3 kan du bare kopiere din i3 konfigurasjon til
`~/.config/sway/config`. Ellers skal du kopiere eksempel konfigurasjonsfilen til
`~/.config/sway/config`. Eksempel filen er normalt plasert i `/etc/sway/config`.  Kjør
`man 5 sway` for å få oplysninger om konfigurasjonen.

## Utførelse

Kjør `sway` fra en TTY. Noen display managers kan fungere, men Sway har ikke
støtte for dem (gdm er kjent for å fungere ganske bra).

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
