# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - **[Polski][pl]** - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway jest kompozytorem [Wayland] kompatybilnym z [i3].
Przeczytaj [FAQ]. Dołącz do [kanału IRC][IRC channel]
(#sway na irc.libera.chat).

## Podpisy cyfrowe wydań

Wydania są podpisywane przy pomocy klucza [E88F5E48]
i publikowane [na GitHubie][Github releases].

## Instalacja

### Z pakietów

Sway jest dostępny w wielu dystybucjach. Spróbuj zainstalować pakiet "sway" w swoim
menedżerze pakietów. Jeśli nie jest dostępny, sprawdź [tę stronę wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
aby uzyskać informacje dotyczące instalacji w swojej dystrybucji.

Jeśli chciałbyś stworzyć pakiet sway dla swojej dystrybucji, odwiedź kanał IRC lub wyślij email na
adres sir@cmpwn.com w celu uzyskania wskazówek.

### Kompilacja ze Źródła

Zainstaluj zależności:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (opcjonalnie: system tray)
* [scdoc] (opcjonalnie: strony pomocy man) \*
* git \*

_\*zależności kompilacji_

Wykonaj następujące polecenia:

    meson build
    ninja -C build
    sudo ninja -C build install

Na systemach bez logind należy wykonać polecenie suid na pliku wykonywalnym sway:

    sudo chmod a+s /usr/local/bin/sway

Sway pozbędzie się uprawnień roota tuż po wystartowaniu.

## Konfiguracja

Jeśli już korzystasz z i3, skopiuj swoją konfigurację i3 do katalogu `~/.config/sway/config` i
zadziała od ręki. W przeciwnym razie skopiuj przykładowy plik konfiguracyjny do folderu
`~/.config/sway/config`; zazwyczaj znajduje się w `/etc/sway/config`.
Wykonaj polecenie `man 5 sway` aby uzyskać informacje dotyczące konfiguracji.

## Uruchamianie

Wykonaj polecenie `sway` z poziomu TTY. Niektóre menedżery wyświetlania mogą umożliwiać rozruch z ich
poziomu, ale nie jest to wspierane przez sway (w gdm podobno działa to całkiem nieźle).

[en]: https://github.com/swaywm/sway#readme
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
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
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
