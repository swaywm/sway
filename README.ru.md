# sway

Sway на данный момент **(в разработке)** i3-совместимый
[Wayland](http://wayland.freedesktop.org/) композитор.  Прочитайте
[FAQ](https://github.com/swaywm/sway/wiki). Присоединяйтесь к [IRC
каналу](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway на
irc.freenode.net).

При желании поддержать разработку Sway вы можете пожертвовать [автору
на его Patreon странице](https://patreon.com/sircmpwn). Вы также можете объявить свою награду за определённую цель и больше всего для этого подходит Patreon.

## Помощь

DarkReef оказывает поддержку на русском языке в IRC канале и на GitHub в часовом поясе UTC +05:00.
Если у вас есть желание помочь с переводом на русский язык, то, пожалуйста, ознакомьтесь с [подсказками для переводчиков](https://github.com/swaywm/sway/issues/1318). На этой же странице можно узнать [статус перевода](https://github.com/swaywm/sway/issues/1318#issuecomment-326913020).

## Подпись версий

Версии подписаны ключом [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
и опубликованы [на GitHub'е](https://github.com/swaywm/sway/releases).

## Установка

### Из пакета

Sway доступен во многих дистрибутивах и находится в официальных репозиториях. Попробуйте установить "sway" через ваш пакетный менеджер.
В случае, если это не представляется возможным, то обратитесь к [этой странице](https://github.com/swaywm/sway/wiki/Unsupported-packages)
для получения инструкций по установке для вашего дистрибутива.

Если вы заинтересованы в создании пакета "sway" в вашем дистрибутиве, то сообщите об этом в IRC
канале или отправьте письмо [sir@cmpwn.com](mailto:sir@cmpwn.com).

### Сборка из исходников

See [DEPENDENCIES.md](DEPENDENCIES.md)

_\*Требуется только для swaybar, swaybg_

Выполните следующие команды:

    meson build
    ninja -C build
    sudo ninja -C build install

Иначе:

    sudo chmod a+s /usr/local/bin/sway

## Настройка

Если вы уже используете i3, тогда просто скопируйте ваш конфиг в `~/.config/sway/config`.
В любом другом случае, скопируйте `/etc/sway/config` в `~/.config/sway/config`.
Для более детальной информации о настройке: `man 5 sway`.

## Запуск

Выполните 'sway' в терминале. **Некоторые** менеджеры сессий могут работать, но не поддерживаются sway (к примеру, gdm работает со sway без проблем).
