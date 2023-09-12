# Sway

Sway er en [i3]-kompatibel [Wayland] compositor. Les [Ofte stilte spørsmål].
Delta på [IRC kanalen][IRC kanal] \(#sway på irc.libera.chat).

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

Se [denne wiki-siden][Oppsetting for utvikling] hvis du vil bygge fra HEAD grenen av sway og
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

## Konfigurasjon

Hvis du allerede bruker i3 kan du bare kopiere din i3 konfigurasjon til
`~/.config/sway/config`. Ellers skal du kopiere eksempel konfigurasjonsfilen til
`~/.config/sway/config`. Eksempel filen er normalt plasert i `/etc/sway/config`.  Kjør
`man 5 sway` for å få oplysninger om konfigurasjonen.

## Utførelse

Kjør `sway` fra en TTY. Noen display managers kan fungere, men Sway har ikke
støtte for dem (gdm er kjent for å fungere ganske bra).

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[Ofte stilte spørsmål]: https://github.com/swaywm/sway/wiki
[IRC kanal]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Oppsetting for utvikling]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
