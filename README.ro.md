# sway

sway este un compositor pentru [Wayland](http://wayland.freedesktop.org/) compatibil cu [i3](https://i3wm.org/).
Citiți [FAQ](https://github.com/swaywm/sway/wiki)-ul. Connectați-vă la canalul nostru [IRC](https://web.libera.chat/gamja/?channels=#sway) (#sway pe irc.libera.chat).

## Semnarea digitală

Noile versiuni sunt semnate cu [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)
și postate [pe GitHub](https://github.com/swaywm/sway/releases).

## Instalare

### Din pachete (packages) 

sway este disponibil în multe distribuții. Încercați să instalați pachetul "sway" pe distribuția voastră. Dacă nu este disponibil, uitați-vă în [această pagină wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
pentru informații a cum puteți să instalați pentru distribuția voastră.

Dacă sunteți interesați in a crea pachete pentru distribuția voastră, informați-ne prin IRC sau contactați prin email pe sir@cmpwn.com pentru ajutor.

### Compilare din sursă

Dependențe pentru instalare:

* meson \*
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (opțional, dacă doriți să aveți system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (opțional, pentru paginile man) \*
* git (opțional, pentru informații de versiune) \*

*Dependențe doar pentru compilare*

Rulați aceste comenzi:

```
    meson build
    ninja -C build
    sudo ninja -C build install
```

Pe sisteme fără logind, trebuie să folosiți următoarea comandă pentru a marca binarul de Sway ca suid:

```
    sudo chmod a+s /usr/local/bin/sway
```

Imediat după pornire, Sway va renunța la permisiunile de root.

## Configurare

Dacă folosiți deja i3, copiați fișierul de configurare din i3 în `~/.config/sway/config`, și va funcționa fără a necesita nici o modificare. In caz contrar, copiați exemplul de configurare (disponibil de obicei în `/etc/sway/config`) în `~/.config/sway/config`.
Folosiți comanda `man 5 sway` pentru informații despre configurare.

## Lansare

Folosiți comanda `sway` într-un TTY. Managerii de display nu sunt suportați de către Sway, dar unii pot functiona (se știe că gdm functioneazâ destul de bine).
