# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Dona con fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

[**English**](https://github.com/SirCmpwn/sway/blob/master/README.md#sway--) - [日本語](https://github.com/SirCmpwn/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/SirCmpwn/sway/blob/master/README.de.md#sway--)

"**S**irCmpwn's **Way**land compositor" es un compositor de [Wayland](http://wayland.freedesktop.org/) 
compatible con i3 **en desarrollo**.
Lee el [FAQ](https://github.com/SirCmpwn/sway/wiki). Únete al
[canal IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway en
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Si quieres soportar el desarrollo de Sway, puedes contribuir a [mi página de
Patreon](https://patreon.com/sircmpwn) o puedes contribuir [recompensas](https://github.com/SirCmpwn/sway/issues/986)
para funcionalidades concretas. 
Cualquier contribuidor puede solicitar la recompensa y puedes crear una recompensa
para cualquier funcionalidad que desees, y Patreon es más efectivo para soportar
la salud general del proyecto y el mantenimiento de Sway.

## Firmas Digitales de los Lanzamientos

Las versiones lanzadas son firmadas con [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
y publicadas [en GitHub](https://github.com/SirCmpwn/sway/releases).

## Estado

- [Soporte de las funciones de i3](https://github.com/SirCmpwn/sway/issues/2)
- [Soporte de las funciones IPC](https://github.com/SirCmpwn/sway/issues/98)
- [Soporte de las funciones de i3bar](https://github.com/SirCmpwn/sway/issues/343)
- [Soporte de las funciones de i3-gaps](https://github.com/SirCmpwn/sway/issues/307)
- [Características de seguridad](https://github.com/SirCmpwn/sway/issues/984)

## Instalación

### Desde Paquetes

Sway está disponible en muchas distribuciones. Intenta instalar el paquete "sway"
para la tuya. Si no está disponible, comprueba [esta página de la wiki](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages)
para información sobre cómo instalar Sway en tu distribución.

Si quieres empaquetar Sway para tu distribución, pásate por el canal de IRC o 
envía un correo a sir@cmpwn.com para obtener más consejos.

### Compilar desde Código Fuente

Instala las dependencias:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* asciidoc
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 *
* pam **
* imagemagick (necesario para capturar imágenes con swaygrab)
* ffmpeg (necesario para capturar video con swaygrab)

_\*Sólo es necesario para swaybar, swaybg, y swaylock_

_\*\*Sólo es necesario para swaylock_

Ejecuta estos comandos:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

En sistemas con logind, necesitas darle estas *capabilities* al binario de sway:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

En sistemas que no tienen logind, tienes que poner la bandera de *suid* en el binario de sway:

    sudo chmod a+s /usr/local/bin/sway

## Configuración

Si ya utilizas i3, sólo necesitas copiar tu configuración de i3 a `~/.config/sway/config`
y funcionará automágicamente. Si no, copia la configuración ejemplar a `~/.config/sway/config`.
Normalmente está ubicada en `/etc/sway/config`. Ejecuta `man 5 sway` para más información
sobre la configuración.

Mis propios *dotfiles* están disponibles [aquí](https://git.sr.ht/~sircmpwn/dotfiles) si
quieres un poco de inspiración, y te recomiendo que compruebes la [wiki](https://github.com/SirCmpwn/sway/wiki)
también.

## Running

Ejecuta `sway` desde una TTY. Algunos gestores de pantalla pueden funcionar pero no son soportados
por Sway (gdm funciona bastante bien).
