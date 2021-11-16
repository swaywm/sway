# sway

sway es un compositor de [Wayland](http://wayland.freedesktop.org/) compatible con [i3](https://i3wm.org/).
Lea el [FAQ](https://github.com/swaywm/sway/wiki). Únase al [canal de IRC](https://web.libera.chat/gamja/?channels=#sway) (#sway on
irc.libera.chat).

## Firmas de las versiones

Las distintas versiones están firmadas con [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)
y publicadas [en GitHub](https://github.com/swaywm/sway/releases).

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
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages) \*
* git \*

_\*Compile-time dep_

Desde su consola, ejecute las órdenes:

    meson build
    ninja -C build
    sudo ninja -C build install

En sistemas sin `logind`, necesitará cambiar los permisos del archivo compilado de sway:

    sudo chmod a+s /usr/local/bin/sway

Sway abandonará los permisos de super-usuario al poco de arrancar.

## Configuración

Si ya utiliza i3, copie su archivo de configuración de i3 a `~/.config/sway/config` y
sway funcionará sin tener que configurar nada más. En otro caso, copie el archivo de
configuración básico a `~/.config/sway/config`, normalmente se encuentra en `/etc/sway/config`.
Ejecute `man 5 sway` para obtener información sobre la configuración.

## Ejecución

Ejecute `sway` desde su consola. Algunos gestores de pantalla pueden funcionar sin estar 
soportados por `sway` (sabemos que gdm funciona bastante bien).
