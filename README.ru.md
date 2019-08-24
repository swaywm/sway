# sway

sway - это [i3](https://i3wm.org/)-совместимый [Wayland](http://wayland.freedesktop.org/) композитор.
Больше информации на [FAQ](https://github.com/swaywm/sway/wiki). Присоединяйтесь к [IRC
каналу](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway на
irc.freenode.net).

Если вы хотите поддержать разработку sway, сделайте пожертвование на [Patreon
странице SirCmpwn](https://patreon.com/sircmpwn).

## Подписи релизов

Релизы подписываются с [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
и публикуются [на GitHub](https://github.com/swaywm/sway/releases).

## Установка

### Из репозиториев

Sway доступен во многих дистрибутивах. Попробуйте установить пакет "sway"
в вашем. Если он вдруг не доступен, проверьте [эту страницу на wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
для более детальной информации о подробностях установки для вашего
дистрибутива.

Если вы заинтересованы поддерживать sway в вашем дистрибутиве, загляните в наш IRC канал
или обратитесь на sir@cmpwn.com за советом.

### Сборка из исходников

Установите зависимости:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (необязательно: для работы трея)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (необязательно: для сборки man страниц) \*
* git \*

_\*Зависимости для сборки_

Выполните эти комманды:

    meson build
    ninja -C build
    sudo ninja -C build install

На системах без logind, вам понадобиться добавить suid к файлу программы sway:

    sudo chmod a+s /usr/local/bin/sway

Sway сбросит root-права при запуске.

## Настройка

Если вы уже используете i3, тогда скопируйте ваш конфигурационный файл i3 в `~/.config/sway/config` и
он сразу же заработает. В противном случае, скопируйте образец конфигурационного файла в
`~/.config/sway/config`. Он обычно располагается в `/etc/sway/config`.
Запустите `man 5 sway` для изучения информации о настройке.

## Запуск

Выполните команду `sway` прямо из TTY. Некоторые дисплейные менеджеры могут работать, но они не поддерживаются со стороны
sway (gdm работает довольно хорошо).
