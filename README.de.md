# Sway
Sway ist ein [i3]-kompatibler [Wayland]-Compositor. Lies die [FAQ]. Tritt dem [IRC Channel] bei (#sway on irc.libera.chat; Englisch).

## Signaturen
Jeder Release wird mit dem PGP-Schlüssel [E88F5E48] signiert und [auf GitHub][GitHub releases] veröffentlicht.

## Installation

### Über die Paketverwaltung

Sway kann in vielen Distributionen direkt durch die Paketverwaltung installiert werden. Versuche einfach das Paket "sway" zu installieren.

### Quellcode selbst kompilieren

sway benötigt die folgenden Pakete:

* meson \*
* [wlroots]
* wayland
* wayland-protocols\*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (Optional, wird für das Benachrichtigungsfeld (System Tray) benötigt)
* [swaybg] (Optional, wird für das Setzen von Desktophintergrundbildern benötigt)
* [scdoc] (Optional, wird für die Dokumentation (Man Pages) benötigt)\*
* git (Optional: Versionsinfo)\*

_\*Werden nur für das Kompilieren benötigt_

Führe die folgenden Befehle aus:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

Schaue in das [Wiki][Development setup] (Englisch) für Informationen, falls du zum Testen oder Entwickeln den neuesten Stand (HEAD) von sway und wlroots kompilieren willst.

## Konfiguration

Falls du von i3 migrierst, kannst du deine Konfigurationsdatei nach `~/.config/sway/config` kopieren und die Einstellungen sollten ohne Weiteres funktionieren. Ansonsten kannst du die Beispielkonfiguration, die normalerweise in `/etc/sway/config` liegt, nach `~/.config/sway/config` kopieren. Die Dokumentation zur Konfigurationsdatei findest du in `man 5 sway`.

## Sway starten
Sway kann einfach mit dem Befehl `sway` vom TTY oder mithilfe eines Displaymanagers gestartet werden.

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
