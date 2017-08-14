# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Mit fosspay spenden](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

Der Fortschritt dieser Übersetzung kann [hier](https://github.com/SirCmpwn/sway/issues/1318) 
eingesehen werden.

"**S**irCmpwn's **Way**land compositor" ist ein i3-kompatibler 
[Wayland](http://wayland.freedesktop.org/)-Kompositor. Lies die 
[FAQ](https://github.com/SirCmpwn/sway/wiki#faq). Tritt dem 
[IRC-Channel](http://webchat.freenode.net/?channels=sway&uio=d4) bei (#sway in irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Falls du die Sway Entwicklung unterstützen möchtest, kannst du das auf der 
[Patreonseite](https://patreon.com/sircmpwn) tun, oder indem du zu
[Entwicklungsprämien](https://github.com/SirCmpwn/sway/issues/986) 
bestimmter Features beiträgst. Jeder ist dazu eingeladen, eine Prämie in Anspruch
zu nehmen oder für gewünschte Features bereitzustellen. Patreon ist eher dafür
gedacht, Sways Wartung und das Projekt generell zu unterstützen.

## Deutscher Support

refacto(UTC+2) bietet Support im IRC (unter dem Namen azarus) und auf Github an.
ParadoxSpiral(UTC+2) bietet Support im IRC und auf Github an.

## Releasesignaturen

Neue Versionen werden mit 
[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A) 
signiert und [auf Github](https://github.com/SirCmpwn/sway/releases) veröffentlicht.

## Status

- [i3-Features](https://github.com/SirCmpwn/sway/issues/2)
- [IPC-Features](https://github.com/SirCmpwn/sway/issues/98)
- [i3bar-Features](https://github.com/SirCmpwn/sway/issues/343)
- [i3-gaps-Features](https://github.com/SirCmpwn/sway/issues/307)
- [Sicherheitsfeatures](https://github.com/SirCmpwn/sway/issues/984)

## Installation

### Als Paket

Sway ist in vielen Distributionen verfügbar: versuche einfach, das "sway"-Paket
zu installieren. Falls es nicht vorhanden ist, schau dir 
[diese Wikiseite](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages) für 
distributionsspezifische Installationsinformationen an.

Wenn du Interesse hast, Sway für deine Distribution als Paket bereitzustellen, 
schau im IRC-Channel vorbei oder schreibe eine e-Mail an sir@cmpwn.com (nur englischsprachig).

### Kompilieren des Quellcodes

Abhängigkeiten:

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
* imagemagick (erforderlich für Bildaufnahme mit swaygrab)
* ffmpeg (erforderlich für Videoaufnahme swaygrab)

_\*Nur erforderlich für swaybar, swaybg, und swaylock_

_\*\*Nur erforderlich für swaylock_

Führe diese Befehle aus:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

In Systemen mit logind musst du `sway` einige Capabilities geben:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

In Systemen ohne logind musst du `sway` das suid-Flag geben:

    sudo chmod a+s /usr/local/bin/sway

## Konfiguration

Wenn du schon i3 benutzt, kopiere einfach deine i3 Konfiguration nach
`~/.config/sway/config`. Falls nicht, kannst du die Beispielkonfiguration
benutzen. Die befindet sich normalerweise unter `/etc/sway/config`.
Um mehr Informationen über die Konfiguration zu erhalten, führe `man 5 sway` aus.

## Verwendung

Führe `sway` von einem TTY aus. Manche Displaymanager könnten funktionieren, werden aber
nicht von Sway unterstützt (gdm scheint relativ gut zu funktionieren).
