# Sway

Sway er en [i3]-kompatibel [Wayland] compositor. Læs [Ofte stillede spørgsmål].
Deltag på [IRC kanalen][IRC kanal] \(#sway på irc.libera.chat).

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

Se [denne wiki-side][Opsætning til udvikling] hvis du vil bygge HEAD af sway og
wlroots til test eller udvikling.

Installationsafhængigheder:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre
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

På systemer uden logind eller seatd skal du sætte SUID bit på sway filen:

    sudo chmod a+s /usr/local/bin/sway

Sway dropper 'root' tilladelser kort efter opstart.

## Konfiguration

Hvis du allerede bruger i3 kan du bare kopiere din i3 konfiguration til
`~/.config/sway/config`. Ellers skal du kopiere eksempelkonfigurationsfilen til
`~/.config/sway/config`. Den er normalt placeret i `/etc/sway/config`.  Kør
`man 5 sway` for at få oplysninger om konfigurationen.

## Eksekvering

Kør `sway` fra en TTY. Nogle display managers kan fungere, men Sway yder ikke
support til dem (gdm er kendt for at fungere temmelig godt).

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[Ofte stillede spørgsmål]: https://github.com/swaywm/sway/wiki
[IRC kanal]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Opsætning til udvikling]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
