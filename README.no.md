# Sway

Sway er en [i3]-kompatibel [Wayland]-compositor. Les [Ofte stilte spørsmål].
Delta på [IRC-kanalen][IRC-kanal] \(#sway på irc.libera.chat).

## Signaturer

Utgivelser er signert med [E88F5E48] og publisert [på GitHub][GitHub releases].

## Installasjon

### Fra systempakker

Sway er tilgjengelig i mange distribusjoner. Prøv å installere pakken "sway"
fra din distro sine repoer.

### Kompilering fra kildekode

Se [denne wiki-siden][Oppsetting for utvikling] hvis du vil bygge fra HEAD-grenen av
sway og wlroots for testing eller utvikling.

Installer avhengigheter:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (valgfritt: støtte for ekstra bildeformater i system tray)
* [swaybg] (valgfritt: bakgrunnsbilde)
* [scdoc] (valgfritt: man pages) \*
* git (valgfritt: versjonsinformasjon) \*

_\* Kompileringsavhengigheter_

Kjør følgende kommandoer:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## Konfigurasjon

Hvis du allerede bruker i3 kan du bare kopiere din i3-konfigurasjon til
`~/.config/sway/config`. Ellers skal du kopiere eksempelkonfigurasjonsfilen til
`~/.config/sway/config`. Eksempelfilen er normalt plasert i `/etc/sway/config`.
Kjør `man 5 sway` for å få opplysninger om konfigurasjonen.

## Kjøring

Kjør `sway` fra en TTY eller fra en display manager.

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[Ofte stilte spørsmål]: https://github.com/swaywm/sway/wiki
[IRC-kanal]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Oppsetting for utvikling]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
