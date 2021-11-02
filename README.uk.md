# sway

Sway це сумісний з [i3](https://i3wm.org/) композитор [Wayland](http://wayland.freedesktop.org/).
Ознайомтесь з [ЧаПами](https://github.com/swaywm/sway/wiki). Приєднуйтесь до [спільноти в
IRC](https://web.libera.chat/gamja/?channels=#sway) (#sway на
irc.libera.chat).

## Підтримка українською мовою

Якщо ви хочете отримати підтримку українською мовою, можете звернутись до користувача
Hummer12007 у IRC-спільноті. Будьте терплячі, вам обов'язково допоможуть.

Наразі переклад Sway українською ще не завершено (він неповний), проте у вас є шанс долучитись,
детальніше див. [статус](https://github.com/swaywm/sway/issues/1318#issuecomment-322277382).

## Підписи випусків

Випуски підписані ключем [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)
та публікуються на сторінці [GitHub](https://github.com/swaywm/sway/releases).

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
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (необов'язково, необхідно для сторінок man) \*
* git \*

_\*Лише для компіляції_

Виконайте ці команди:

    meson build
    ninja -C build
    sudo ninja -C build install

На системах без logind, необхідно встановити біт SUID на виконуваний файл sway:

    sudo chmod a+s /usr/local/bin/sway

Sway втратить права доступу root незабаром після запуску.

## Налаштування

Якщо ви вже використовуєте i3, скопіюйте свій файл налаштувань
до `~/.config/sway/config`, він має запрацювати. Інакше, скопіюйте
туди файл-зразок (зазвичай знаходиться у `/etc/sway/config`), і налаштуйте під себе.

Більше інформації щодо налаштувань можете знайти, виконавши `man 5 sway`.

## Запуск

Виконайте `sway` у TTY. Деякі дисплейні менеджери (менеджери сеансу/стільниць)
можуть працювати, але офіційно не підтримуються (проте сумісніть із gdm достатньо висока).
