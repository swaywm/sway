# sway

Sway is een [i3](https://i3wm.org/)-compatibele [Wayland](http://wayland.freedesktop.org/) compositor.
Lees de [FAQ](https://github.com/swaywm/sway/wiki). Word lid van het [IRC
kanaal](https://web.libera.chat/gamja/?channels=#sway) (#sway op
irc.libera.chat).

## Releasehandtekeningen

Releases worden ondertekend met [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)
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
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
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
