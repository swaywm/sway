# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - **[Українська][uk]** - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Sway це сумісний з [i3] композитор [Wayland].
Ознайомтесь з [ЧаПами](https://github.com/swaywm/sway/wiki). Приєднуйтесь до [спільноти в
IRC][IRC channel] (#sway на
irc.libera.chat).

## Підтримка українською мовою

Якщо ви хочете отримати підтримку українською мовою, можете звернутись до користувача
Hummer12007 у IRC-спільноті. Будьте терплячі, вам обов'язково допоможуть.

Наразі переклад Sway українською ще не завершено (він неповний), проте у вас є шанс долучитись,
детальніше див. [статус](https://github.com/swaywm/sway/issues/1318#issuecomment-322277382).

## Підписи випусків

Випуски підписані ключем [E88F5E48]
та публікуються на сторінці [GitHub][Github releases].

## Встановлення

### З пакунків

Sway доступний у багатьох дистрибутивах Linux (а також у FreeBSD).
Спробуйте встановити пакунок `sway` у вашому.
Якщо він недоступний, перегляньте цю [сторінку Wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
для інформації щодо встановлення на вашому дистрибутиві.

Якщо ви готові та зацікавлені запакувати і підтримувати Sway у вашому
дистрибутиві, звертайтесь за порадами до нашого каналу в IRC або
пишіть на електронну пошту [sir@cmpwn.com](mailto:sir@cmpwn.com).

### З вихідного коду

Встановіть залежності:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: system tray)
* [scdoc] (необов'язково, необхідно для сторінок man) \*
* git \*

_\*Лише для компіляції_

Виконайте ці команди:

    meson build
    ninja -C build
    sudo ninja -C build install

## Налаштування

Якщо ви вже використовуєте i3, скопіюйте свій файл налаштувань
до `~/.config/sway/config`, він має запрацювати. Інакше, скопіюйте
туди файл-зразок (зазвичай знаходиться у `/etc/sway/config`), і налаштуйте під себе.

Більше інформації щодо налаштувань можете знайти, виконавши `man 5 sway`.

## Запуск

Виконайте `sway` у TTY. Деякі дисплейні менеджери (менеджери сеансу/стільниць)
можуть працювати, але офіційно не підтримуються (проте сумісніть із gdm достатньо висока).

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
