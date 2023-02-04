# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - **[Français][fr]** - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Sway est un compositeur [Wayland] compatible avec [i3]. Lisez la
[FAQ]. Rejoignez le [canal IRC][IRC channel] (#sway sur irc.libera.chat).

## Aide en français

[abdelq] fournit du support en français sur IRC et Github, dans le fuseau
horaire UTC-4 (EST).

## Signatures de nouvelles versions

Les nouvelles versions sont signées avec [E88F5E48] et publiées
[sur GitHub][GitHub releases].

## Installation

### À partir de paquets

Sway est disponible sur beaucoup de distributions. Essayez d'installer le
paquet "sway" pour la vôtre.

Si vous êtes intéressé à maintenir Sway pour votre distribution, passez sur le
canal IRC ou envoyez un e-mail à sir@cmpwn.com (en anglais seulement) pour des
conseils.

### Compilation depuis les sources

Consultez [cette page wiki][Development setup] si vous souhaitez
compiler la révision HEAD de sway et wlroots pour tester ou développer.

Installez les dépendances :

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (optionnel : system tray)
* [scdoc] (optionnel : requis pour les pages man) \*
* git (optionnel : information de version) \*

_\* Requis uniquement pour la compilation_

Exécutez ces commandes :

    meson build
    ninja -C build
    sudo ninja -C build install

Sur les systèmes sans logind, vous devez suid le binaire de sway :

    sudo chmod a+s /usr/local/bin/sway

Sway se débarassera des permissions *root* peu de temps après le démarrage.

## Configuration

Si vous utilisez déjà i3, copiez votre configuration i3 vers
`~/.config/sway/config` et sway fonctionnera directement. Sinon, copiez
l'exemple de fichier de configuration vers `~/.config/sway/config`. Il se
trouve généralement dans `/etc/sway/config`. Exécutez `man 5 sway` pour lire la
documentation pour la configuration de sway.

## Exécution

Exécutez `sway` à partir d'un TTY. Certains gestionnaires d'affichage peuvent
fonctionner, mais ne sont pas supportés par Sway (gdm est réputé pour assez
bien fonctionner).

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

[abdelq]: https://github.com/abdelq
