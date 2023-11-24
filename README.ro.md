# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - **[Română][ro]** - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway este un compositor pentru [Wayland] compatibil cu [i3].
Citiți [FAQ]-ul. Connectați-vă la canalul nostru [IRC][IRC channel] (#sway pe irc.libera.chat).

## Semnarea digitală

Noile versiuni sunt semnate cu [E88F5E48]
și postate [pe GitHub][Github releases].

## Instalare

### Din pachete (packages) 

sway este disponibil în multe distribuții. Încercați să instalați pachetul "sway" pe distribuția voastră. Dacă nu este disponibil, uitați-vă în [această pagină wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
pentru informații a cum puteți să instalați pentru distribuția voastră.

Dacă sunteți interesați in a crea pachete pentru distribuția voastră, informați-ne prin IRC sau contactați prin email pe sir@cmpwn.com pentru ajutor.

### Compilare din sursă

Dependențe pentru instalare:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (opțional, dacă doriți să aveți system tray)
* [scdoc] (opțional, pentru paginile man) \*
* git (opțional, pentru informații de versiune) \*

*Dependențe doar pentru compilare*

Rulați aceste comenzi:

```
    meson build
    ninja -C build
    sudo ninja -C build install
```

## Configurare

Dacă folosiți deja i3, copiați fișierul de configurare din i3 în `~/.config/sway/config`, și va funcționa fără a necesita nici o modificare. In caz contrar, copiați exemplul de configurare (disponibil de obicei în `/etc/sway/config`) în `~/.config/sway/config`.
Folosiți comanda `man 5 sway` pentru informații despre configurare.

## Lansare

Folosiți comanda `sway` într-un TTY. Managerii de display nu sunt suportați de către Sway, dar unii pot functiona (se știe că gdm functioneazâ destul de bine).

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
