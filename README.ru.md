# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Пожертвовать через fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor" на данный момент **(в разработке)**
i3-совместимый [Wayland](http://wayland.freedesktop.org/) композитор.
Прочитайте [FAQ](https://github.com/SirCmpwn/sway/wiki). Присоединяйтесь к
[IRC каналу](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway на
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

При желании поддержать разработку Sway вы можете пожертвовать [автору
на его Patreon странице](https://patreon.com/sircmpwn) или взяться
за разработку определённых целей в обмен на [награду](https://github.com/SirCmpwn/sway/issues/986).

Вы также можете объявить свою награду за определённую цель и больше всего для этого подходит Patreon.

## Помощь

DarkReef оказывает поддержку на русском языке в IRC канале и на GitHub в часовом поясе UTC +05:00.
Если у вас есть желанием помочь с переводом на русский языке, то, пожалуйста, ознакомьтесь с [подсказками для переводчиков](https://github.com/SirCmpwn/sway/issues/1318). На этой же странице можно узнать [статус перевода](https://github.com/SirCmpwn/sway/issues/1318#issuecomment-326913020).

## Подпись версий

Версии подписаны ключом [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
и опубликованы [на GitHub'е](https://github.com/SirCmpwn/sway/releases).

## Статус

- [Поддержка i3](https://github.com/SirCmpwn/sway/issues/2)
- [Поддержка i3-bar](https://github.com/SirCmpwn/sway/issues/343)
- [Поддержка i3-gaps](https://github.com/SirCmpwn/sway/issues/307)
- [Поддержка IPC](https://github.com/SirCmpwn/sway/issues/98)
- [Безопасность](https://github.com/SirCmpwn/sway/issues/984)

## Установка

### Из пакета

Sway доступен во многих дистрибутивах и находится в официальных репозиториях. Попробуйте установить "sway" через ваш пакетный менеджер.
В случае, если это не представляется возможным, то обратитесь к [этой странице](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages)
для получения инструкций по установке для вашего дистрибутива.

Если вы заинтересованы в создании пакета "sway" в вашем дистрибутиве, то сообщите об этом в IRC
канале или отправьте письмо sir@cmpwn.com.

### Сборка из исходников

Установите следующие пакеты:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* asciidoc
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 *
* pam **
* imagemagick (требуется для захвата изображений через swaygrab)
* ffmpeg (требуется для захвата видео через swaygrab)

_\*Требуется только для swaybar, swaybg и swaylock_

_\*\*Требуется только для swaylock_

Выполните следующие команды:

	mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

Если у вас logind:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/bin/sway

Иначе:

    sudo chmod a+s /usr/local/bin/sway

## Настройка

Если вы уже используете i3, тогда просто скопируйте ваш конфиг в `~/.config/sway/config`.
В любом другом случае, скопируйте `/etc/sway/config` в `~/.config/sway/config`.
Для более детальной информации о настройке: `man 5 sway`.

## Запуск

Выполните 'sway' в терминале. **Некоторые** менеджеры сессий могут работать, но не поддерживаются sway (К примеру, gdm работает со sway без проблем).
