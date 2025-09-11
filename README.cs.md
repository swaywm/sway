# sway

sway je [waylandový][Wayland] kompozitor kompatibilní s [i3]. Přečtěte si
[FAQ (anglicky)][FAQ]. Připojte se na [IRC kanál (anglicky)][IRC channel]
\(#sway na irc.libera.chat).

## Podpisy vydání

Vydané verze jsou podepsány klíčem [E88F5E48] a publikovány
[na GitHubu][GitHub releases].

## Instalace

### Z balíků

Sway je dostupný v mnoha distribucích. Zkuste v té vaší nainstalovat balík "sway".

### Kompilace ze zdrojových kódů

Pokud chcete sestavit HEAD repozitáře sway a wlroots pro testování nebo vývoj,
použijte návod na [této stránce na wiki (anglicky)][Development setup].

Nainstalujte závislosti:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (volitelné: dodatečné formáty ikon pro oznamovací oblast)
* [swaybg] (volitelné: tapeta plochy)
* [scdoc] (volitelné: man stránky) \*
* git (volitelné: informace o verzi) \*

_\* Závislost pouze pro kompilaci_

Spusťte tyto příkazy:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## Konfigurace

Pokud již používáte i3, zkopírujte svou konfiguraci i3 do `~/.config/sway/config`
a ta bude ihned fungovat. Jinak zkopírujte do `~/.config/sway/config` ukázkový
konfigurační soubor. Ten se obvykle nachází v `/etc/sway/config`.
Pro více informací o konfiguraci spusťte `man 5 sway`.

## Spuštění

Spusťte `sway` z TTY nebo ze správce displeje.

[en]: https://github.com/swaywm/sway#readme
[ar]: README.ar.md
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[ge]: README.ge.md
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
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
