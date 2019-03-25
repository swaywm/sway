# sway

Sway е в процес на разработка, съвместим с i3,
[Wayland](http://wayland.freedesktop.org/) композитор.  Прочетете
[FAQ](https://github.com/swaywm/sway/wiki). Присъединете се в [IRC
канала](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway на
irc.freenode.net).

Ако желаете, може да дарите на [Patreon страницата на автора](https://patreon.com/sircmpwn), което ще помогне за цялостното здраве и развитие на проекта.

## Помощ

Ако имате нужда от помощ - влезте в IRC канала. Ако не ви отговорят в IRC или желаете кореспонденция по и-мейл - [fokditkak](mailto:martin.kalchev@mail.ru)
може да отговаря на основни въпроси на български език.

## Подпис

Версии подписани с ключ [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
и публикувани в [GitHub](https://github.com/swaywm/sway/releases).

## Инсталация

### От пакети

Sway съществува в репотата (хранилищата) на много дистрибуции. Пробвайте да го инсталирате със съответния пакетен мениджър на вашата дистрибуция.
В случай, че имате проблем погледнете [тази страница](https://github.com/swaywm/sway/wiki/Unsupported-packages) за помощ.

Ако желаете да пакетирате Sway за вашата дистрибуция влезте в IRC канала
или пратете и-мейл на [sir@cmpwn.com](mailto:sir@cmpwn.com) за съвет как да го направите.

### Компилиране от сорс-код

See [DEPENDENCIES.md](DEPENDENCIES.md)

_\*Нужен само за swaybar, swaybg_

Изпълнете следните команди:

    meson build
    ninja -C build
    sudo ninja -C build install

Иначе:

    sudo chmod a+s /usr/local/bin/sway

## Настройка

Ако имте съшествуващa конфигурация от i3, просто я копирайте в `/.config/sway/config` и би трябвало безпроблемно да работи.
В противен случай, поставете примерната конфигурация, която би трябвало да се намиреа в `/etc/sway/config`.
Напишете `man 5 sway` за повече информация.

## Пускане

Напишете 'sway' в терминала (TTY). Някои дисплей мениджъри (display managers) може и да работят, но като цяло не са поддържани от Sway.
