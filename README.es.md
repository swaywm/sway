# sway

[**English**](https://github.com/swaywm/sway/blob/master/README.md#sway--) - [日本語](https://github.com/swaywm/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/swaywm/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/swaywm/sway/blob/master/README.el.md#sway--) - [Français](https://github.com/swaywm/sway/blob/master/README.fr.md#sway--) - [Українська](https://github.com/swaywm/sway/blob/master/README.uk.md#sway--) - [Italiano](https://github.com/swaywm/sway/blob/master/README.it.md#sway--) - [Português](https://github.com/swaywm/sway/blob/master/README.pt.md#sway--) -
[Русский](https://github.com/swaywm/sway/blob/master/README.ru.md#sway--) - [Български](https://github.com/swaywm/sway/blob/master/README.bg.md#sway--)

sway es un i3-compatible [Wayland](http://wayland.freedesktop.org/) compositor.
Leer el [FAQ](https://github.com/swaywm/sway/wiki). Participe en el [IRC
canal ](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

Si desea apoyar al desarrolo de sway, por favor contribuir a [SirCmpwn's
Patreon page](https://patreon.com/sircmpwn).

## Claves de autenticidad

Los lanzamientos estan firmados con [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
y publicados [en GitHub](https://github.com/swaywm/sway/releases).

## Instalación

### A partir de paquetes

Sway esta disponible en muchas distribuciones. Probar instalar el paquete "sway".
Si no esta disponible, ver [esta página wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
para obtener infomación de su distribución.

Si está interesado en enpaquetar sway para su distribución, visite el canal IRC
o envie un un correo a sir@cmpwn.com (*en inglês*).

### A partir de código fuente

Antes de iniciar la compilación, instale estas dependencias:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 \*\*
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) >= 1.8.1 (optional: man pages)
* git \*

_\*Dependencia en tiempo compilacion dep_

_\*\*opcional: requerido por swaybg_

Ejecutar estos comandos:

    meson build
    ninja -C build
    sudo ninja -C build install

En systemas sin logind, necesita dar permiso (suid) al binario:

    sudo chmod a+s /usr/local/bin/sway

Sway dejará el permiso de root poco después del inicio.

## Configuración

Si usa i3, puede copiar la configuración i3 a `~/.config/sway/config`,
y este deberia trabajar. En otro caso copiar el archivo por defecto
`/etc/sway/config` a `.config/sway/config` y configure a su conveniencia,
teclado, monitores, atajos de teclado para sus aplicaciones.
Para más información ejecute `man 5 sway`

## Ejecutando

Solo necesita ejecutar `sway` desde la terminal (TTY).
Algunos gestores de ventana (display manager) pueden no trabajar en sway
y entornos wayland. (gdm es uno que funciona bien).
