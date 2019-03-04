# sway

[**English**](https://github.com/swaywm/sway/blob/master/README.md#sway--) - [日本語](https://github.com/swaywm/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/swaywm/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/swaywm/sway/blob/master/README.el.md#sway--) - [Français](https://github.com/swaywm/sway/blob/master/README.fr.md#sway--) - [Українська](https://github.com/swaywm/sway/blob/master/README.uk.md#sway--) - [Italiano](https://github.com/swaywm/sway/blob/master/README.it.md#sway--) - [Português](https://github.com/swaywm/sway/blob/master/README.pt.md#sway--) -
[Русский](https://github.com/swaywm/sway/blob/master/README.ru.md#sway--) - [Български](https://github.com/swaywm/sway/blob/master/README.bg.md#sway--) - [Español](https://github.com/swaywm/sway/blob/master/README.es.md#sway--) -
[Polski](https://github.com/swaywm/sway/blob/master/README.pl.md#sway--)

sway jest kompozytorem [Wayland](http://wayland.freedesktop.org/) kompatybilnym z i3.
Przeczytaj [FAQ](https://github.com/swaywm/sway/wiki). Dołącz do [kanału IRC](http://webchat.freenode.net/?channels=sway&uio=d4)
(#sway na irc.freenode.net).

Jeśli chcesz wesprzeć rozwój sway, rozważ wsparcie SirCmpwn na jego [stronie Patreon](https://patreon.com/sircmpwn).

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
* gdk-pixbuf2 \*\*
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (opcjonalnie: strony pomocy man) \*
* git \*

_\*zależności kompilacji_

_\*\*opcjonalnie: wymagane dla swaybg_

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
