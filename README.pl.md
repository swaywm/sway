# sway

sway jest kompozytorem [Wayland](http://wayland.freedesktop.org/) kompatybilnym z [i3](https://i3wm.org/).
Przeczytaj [FAQ](https://github.com/swaywm/sway/wiki). Dołącz do [kanału IRC](http://webchat.freenode.net/?channels=sway&uio=d4)
(#sway na irc.freenode.net).

## Podpisy cyfrowe wydań

Wydania są podpisywane przy pomocy klucza [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
i publikowane [na GitHubie](https://github.com/swaywm/sway/releases).

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
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (opcjonalnie: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (opcjonalnie: strony pomocy man) \*
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
