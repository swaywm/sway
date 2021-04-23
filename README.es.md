# sway

sway es un compositor de [Wayland](http://wayland.freedesktop.org/) compatible con [i3](https://i3wm.org/).
Lea el [FAQ](https://github.com/swaywm/sway/wiki). Únase al [canal de IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

## Firmas de las versiones

Las distintas versiones están firmadas con [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
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
* [wlroots](https://github.com/swaywm/wlroots)
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
