# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Mit fosspay spenden](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land Kompositor" is a i3-kompatibler
[Wayland](http://wayland.freedesktop.org/) Kompositor.
Liest das [FAQ](https://github.com/SirCmpwn/sway/wiki). Tritt dem
[IRC Channel](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net) zu.

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Falls du die Sway-Entwicklung unterstüzen willst, kannst du meinem 
[Patreon](https://patreon.com/sircmpwn) spenden oder du kannst zu den
[Belohnungen](https://github.com/SirCmpwn/sway/issues/986) spenden (für 
spezifische Funktionen).
Jeder ist dazu eingeladen, eine Belohnung zu beantragen und du kannst eine
Belohnung für jegliche Funktionen erstellen. Patreon ist nützlicher um
Sway generell zu unterstützen.

## Veröffentlichungssignaturen

Veröffentlichungen sind mit 
[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
signiert und sind verfügbar 
[auf GitHub](https://github.com/SirCmpwn/sway/releases).

## Status

- [i3 Funktionsunterstützung](https://github.com/SirCmpwn/sway/issues/2)
- [IPC Funktionsunterstützung](https://github.com/SirCmpwn/sway/issues/98)
- [i3bar Funktionsunterstützung](https://github.com/SirCmpwn/sway/issues/343)
- [i3-gaps Funktionsunterstützung](https://github.com/SirCmpwn/sway/issues/307)
- [Sicherheitsfunktionen](https://github.com/SirCmpwn/sway/issues/984)

## Installation

### Mit Paketen

Sway ist verfügbar in vielen Distributionen. Probier das "sway" Paket für deine
Distribution. Falls es nicht verfügbar ist, lies
[diese Wiki-Seite](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages)
für Informationen über die Installation auf deiner Distribution.

Falls du interessiert bist um Pakete für deine Distribution bereitzustellen,
solltest du den IRC Channel besuchen oder schicke eine E-Mail an sir@cmpwn.com 
zur Beratung.

### Von der Source kompilieren

Installationsabhängigkeiten:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* asciidoc
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 *
* pam **
* imagemagick (benötigt für Bilderaufnahmen mit swaygrab)
* ffmpeg (benötigt für Videoaufnahmen mit swaygrab)

_\*Nur benötigt für swaybar, swaybg, und swaylock_

_\*\*Nur benötigt für swaylock_

Führe diese Befehle aus:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

Auf Systemen mit logind musst du ein paar zusätzliche caps auf die Binärdatei
setzen:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

Auf Systemen ohne logind musst du suid auf die Binärdatei setzen:

    sudo chmod a+s /usr/local/bin/sway

## Konfiguration

Falls du schon i3 brauchst, kannst du deine Konfiguration zu
`~/.config/sway/config` kopieren und es sollte sofort funktionieren. Sonst 
kannst du die Standardkonfigurationsdatei zu `~/.config/sway/config` kopieren.
Normalerweise findet man sie bei `/etc/sway/config`.
Führe `man 5 sway` aus für Informationen relevant zur Konfiguration.

Meine eigenen Konfigurationsdateien kannst du
[hier](https://git.sr.ht/~sircmpwn/dotfiles) finden. Falls du 
Inspiration brauchst, lies die [Wiki](https://github.com/SirCmpwn/sway/wiki).

## Ausführen

Anstatt `startx` auszuführen, führ `sway` aus. Du kannst `sway` auch innerhalb
X ausführen, was nützlich ist um zu testen.
