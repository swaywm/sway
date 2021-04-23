# sway

Sway is een [i3](https://i3wm.org/)-compatibele [Wayland](http://wayland.freedesktop.org/) compositor.
Lees de [FAQ](https://github.com/swaywm/sway/wiki). Word lid van het [IRC
kanaal](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway op
irc.freenode.net).

## Releasehandtekeningen

Releases worden ondertekend met [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
en gepubliceerd [op GitHub](https://github.com/swaywm/sway/releases).

## Installatie

### Via een pakket

Sway is beschikbaar in vele distributies. Probeer het "sway"-pakket te installeren met jouw pakketbeheerapplicatie. Als het niet beschikbaar is, bekijk dan [deze wikipagina](https://github.com/swaywm/sway/wiki/Unsupported-packages)
voor informatie over installatie in jouw distributie.

Als je geïnteresseerd bent in het maken van pakketten voor je distributie, stuur een bericht in het IRC-
kanaal of stuur een e-mail naar sir@cmpwn.com voor advies.

### Compilatie vanuit broncode

Afhankelijkheden installeren:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (optioneel: systeemtray)
* [scdoc](https://git.sr.ht/~ircmpwn/scdoc) (optioneel: manpagina's) \*
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
