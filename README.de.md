# Sway
Sway ist ein [i3](https://i3wm.org/)-kompatibler [Wayland](http://wayland.freedesktop.org/)-Compositor. Lies die [FAQ](https://github.com/swaywm/sway/wiki). Tritt dem [IRC Channel](http://webchat.freenode.net/?channels=sway&uio=d4) bei (#sway on irc.freenode.net; Englisch).

## Signaturen
Jedes Release wird mit dem PGP-Schlüssel [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A) signiert und auf GitHub veröffentlicht.

## Installation
### Mit der Paketverwaltung
Sway kann in vielen Distributionen direkt durch die Paketverwaltung installiert werden. Das Paket sollte "sway" heißen. Falls es kein solches Paket gibt, kannst du im [Wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages) (englisch) nach mehr Informationen bezüglich deiner Distribution suchen.

Falls du sway für deine eigene Distribution als Paket bereitstellen möchtest, solltest du die Entwickler per IRC oder E-Mail (sir@cmpwn.com) kontaktieren.

### Quellcode selbst kompilieren

sway benötigt die folgenden Pakete:

* meson\*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols\*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (Optional, wird für das Benachrichtigungsfeld (System Tray) benötigt)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc)\* (Optional, wird für die Dokumentation (Man Pages) benötigt)
* git\*

_\*Werden nur während des Kompilierens benötigt_

Führe die folgenden Befehle aus:

    meson build
    ninja -C build
    sudo ninja -C build install

Falls dein System nicht logind benutzt, musst du sway noch die passenden Berechtigungen geben:

    sudo chmod a+s /usr/local/bin/sway

Sway läuft nur in der Startphase mit Root-Rechten.

## Konfiguration

Falls du von i3 migrierst, kannst du deine Konfigurationsdatei nach `~/.config/sway/config` kopieren und die Einstellungen sollten ohne Weiteres funktionieren. Ansonsten kannst du die Beispielkonfiguration, die normalerweise in `/etc/sway/config` liegt, nach `~/.config/sway/config` kopieren. Die Dokumentation zur Konfigurationsdatei findest du in `man 5 sway`.

## Sway starten
Sway kann einfach mit dem Befehl `sway` vom TTY gestartet werden.
Display-Manager werden nicht offiziell unterstützt. Es gibt aber durchaus einige, die mit Sway funktionieren (z.B. gdm).
