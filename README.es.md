# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - **[Español][es]** - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway es un compositor de [Wayland] compatible con [i3].
Lea el [FAQ]. Únase al [canal de IRC][IRC channel] (#sway on
irc.libera.chat).

## Firmas de las versiones

Las distintas versiones están firmadas con [E88F5E48]
y publicadas [en GitHub][Github releases].

## Instalación

### Usando paquetes

Sway está disponible en muchas distribuciones. Pruebe instalando el paquete "sway" desde la suya.
Si no está disponible, puede consultar [esta documentación](https://github.com/swaywm/sway/wiki/Unsupported-packages) 
y así obtener información acerca de como instalarlo.

Si está interesado en crear un paquete para su distribución, únase al canal de IRC  o
escriba un email a sir@cmpwn.com

### Compilando el código fuente

Instale las dependencias:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: system tray)
* [scdoc] (optional: man pages) \*
* git \*

_\*Compile-time dep_

Desde su consola, ejecute las órdenes:

    meson build
    ninja -C build
    sudo ninja -C build install

## Configuración

Si ya utiliza i3, copie su archivo de configuración de i3 a `~/.config/sway/config` y
sway funcionará sin tener que configurar nada más. En otro caso, copie el archivo de
configuración básico a `~/.config/sway/config`, normalmente se encuentra en `/etc/sway/config`.
Ejecute `man 5 sway` para obtener información sobre la configuración.

## Ejecución

Ejecute `sway` desde su consola. Algunos gestores de pantalla pueden funcionar sin estar 
soportados por `sway` (sabemos que gdm funciona bastante bien).

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
