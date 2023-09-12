# sway

[English][en] - **[Česky][cs]** - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Svenska][sv] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway je s [i3] kompatibilní [Wayland] kompozitor. Přečtěte si [FAQ]. Připojte se na
[IRC kanál][IRC channel] \(#sway na irc.libera.chat).

## Podpisy vydání

Vydání jsou podepsána [E88F5E48] a publikována [na GitHubu][GitHub releases].

## Instalace

### Z balíčků

Sway je dostupný ve spoustě distribucí. Zkuste nainstalovat balíček "sway" ve vaší
distribuci.

### Kompilace ze zdrojových kódů

Podívejte se na [tuto stránku wiki][Development setup], pokud chcete sestavit HEAD
sway a wlroots pro testování nebo vývoj.

Nainstalujte závislosti:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (volitelné: oznamovací oblast)
* [scdoc] (volitelné: manuálové stránky) \*
* git (volitelné: informace o verzi) \*

_\* Závislost pouze pro sestavení_

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

Spusťte `sway` z TTY. Některé správce zobrazení mohou fungovat, ale nejsou
podporovány sway (je známo, že gdm funguje docela dobře).

[en]: https://github.com/swaywm/sway#readme
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[sv]: README.sv.md
[gr]: README.gr.md
[hi]: README.hi.md
[hu]: README.hu.md
[ir]: README.ir.md
[it]: README.it.md
[ja]: README.ja.md
[ko]: README.ko.md
[nl]: README.nl.md
[pl]: README.pl.md
[pt]: README.pt.md
[ro]: README.ro.md
[ru]: README.ru.md
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
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
