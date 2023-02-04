# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - **[Nederlands][nl]** - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Sway is een [i3]-compatibele [Wayland] compositor.
Lees de [FAQ]. Word lid van het [IRC
kanaal][IRC channel] (#sway op
irc.libera.chat).

## Releasehandtekeningen

Releases worden ondertekend met [E88F5E48]
en gepubliceerd [op GitHub][Github releases].

## Installatie

### Via een pakket

Sway is beschikbaar in vele distributies. Probeer het "sway"-pakket te installeren met jouw pakketbeheerapplicatie. Als het niet beschikbaar is, bekijk dan [deze wikipagina](https://github.com/swaywm/sway/wiki/Unsupported-packages)
voor informatie over installatie in jouw distributie.

Als je geïnteresseerd bent in het maken van pakketten voor je distributie, stuur een bericht in het IRC-
kanaal of stuur een e-mail naar sir@cmpwn.com voor advies.

### Compilatie vanuit broncode

Afhankelijkheden installeren:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (optioneel: systeemtray)
* [scdoc] (optioneel: manpagina's) \*
* git \*

_\* Compileerafhankelijkheden_

Voer deze opdrachten uit:

    meson build
    ninja -C build
    sudo ninja -C build install

Op systemen zonder logind, moet je bij het binaire bestand het suid bit instellen:

    sudo chmod a+s /usr/local/bin/sway

Sway zal root-rechten kort na het opstarten loslaten.

## Configuratie

Als je al i3 gebruikt, kopieer dan je i3-configuratie naar `~/.config/sway/config` en
het zal zonder verdere configuratie werken. Kopieer anders het voorbeeldconfiguratiebestand naar
`~/.config/sway/config`. Dit is meestal het bestand: `/etc/sway/config`.
Voer `man 5 sway` uit voor informatie over het configureren van sway.

## Uitvoeren

Voer `sway` vanaf een TTY uit. Sommige display-managers kunnen werken, maar worden niet ondersteund door
sway (van gdm is bekend dat het redelijk goed werkt).

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
