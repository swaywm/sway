# sway

[English][en] - [Česky][cs] - **[Deutsch][de]** - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Sway ist ein [i3](https://i3wm.org/)-kompatibler [Wayland]-Compositor. Lies die [FAQ]. Tritt dem [IRC Channel] bei (#sway on irc.libera.chat; Englisch).

## Signaturen
Jedes Release wird mit dem PGP-Schlüssel [E88F5E48] signiert und auf GitHub veröffentlicht signiert und [auf GitHub][GitHub releases] veröffentlicht..

## Installation

### Über die Paketverwaltung

Sway kann in vielen Distributionen direkt durch die Paketverwaltung installiert werden. Versuche einfach das Packet "sway" zu installieren.

### Quellcode selbst kompilieren

sway benötigt die folgenden Pakete:

* meson\*
* [wlroots]
* wayland
* wayland-protocols\*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (Optional, wird für das Benachrichtigungsfeld (System Tray) benötigt)
* [scdoc] (Optional, wird für die Dokumentation (Man Pages) benötigt)\*
* git (Optional: Versionsinfo)\*

_\*Werden nur während des Kompilierens benötigt_

Führe die folgenden Befehle aus:

    meson build
    ninja -C build
    sudo ninja -C build install

## Konfiguration

Falls du von i3 migrierst, kannst du deine Konfigurationsdatei nach `~/.config/sway/config` kopieren und die Einstellungen sollten ohne Weiteres funktionieren. Ansonsten kannst du die Beispielkonfiguration, die normalerweise in `/etc/sway/config` liegt, nach `~/.config/sway/config` kopieren. Die Dokumentation zur Konfigurationsdatei findest du in `man 5 sway`.

## Sway starten
Sway kann einfach mit dem Befehl `sway` vom TTY gestartet werden.
Display-Manager werden nicht offiziell unterstützt. Es gibt aber durchaus einige, die mit Sway funktionieren (z.B. gdm).

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
