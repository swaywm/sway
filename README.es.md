# sway [![](https://api.travis-ci.org/swaywm/sway.svg)](https://travis-ci.org/swaywm/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

[**English**](https://github.com/swaywm/sway/blob/master/README.md#sway--) - [日本語](https://github.com/swaywm/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/swaywm/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/swaywm/sway/blob/master/README.el.md#sway--) - [Español](https://github.com/swaywm/sway/blob/master/README.es.md#sway--) - [Français](https://github.com/swaywm/sway/blob/master/README.fr.md#sway--) - [Українська](https://github.com/swaywm/sway/blob/master/README.uk.md#sway--) - [Italiano](https://github.com/swaywm/sway/blob/master/README.it.md#sway--) - [Português](https://github.com/swaywm/sway/blob/master/README.pt.md#sway--) -
[Русский](https://github.com/swaywm/sway/blob/master/README.ru.md#sway--)

"**S**irCmpwn's **Way**land compositor" es un gestor de composición de ventanas
compatible con i3 [Wayland](http://wayland.freedesktop.org/) **en processo de
desarrollo**.
Lee el ["FAQ"](https://github.com/swaywm/sway/wiki). Únete al canal de
["IRC"](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway en
irc.freenode.net).

**Nota**: Sway se encuentra actualmente "congelado"("frozen"), es decir que
nuevas características no están siendo desarrolladas, hasta que terminemos de
integrar Sway y wlroots.
Las correcciones a fallos "Bug fixes" continúan. Para mayor información
[lee éste articulo](https://drewdevault.com/2017/10/09/Future-of-sway.html).
Para mantenerte informado del estado de integración de wlroots, [lee éste
ticket](https://github.com/swaywm/sway/issues/1390).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)


Para el soporte de desarrollo de Sway se puede contribuir a [SirCmpwn's
Patreon](https://patreon.com/sircmpwn) o se puede contribuir mediante el
formato de recompensas ["bounties"](https://github.com/swaywm/sway/issues/986)
para características especificas.
Cualquier persona es bienvenida a contribuir y reclamar una recompensa.
Adicionalmente se pueden crear recompensas("bounties") para cualquier
característica que se desee desarrollar. En general el uso de Patreon es más
útil para el sustento de Sway.

## Ayuda en Español

Al momento no hay ayuda en Español en IRC, [tonyskapunk](https://github.com/tonyskapunk)
puede proveer ayuda en Español en GitHub en el uso horario [UTC-6](https://en.wikipedia.org/wiki/UTC%E2%88%9206:00)

La traducción al Español no esta completa, información sobre el progreso de
la traducción se encuentra documentada en el "issue":
[Documentation i18n](https://github.com/swaywm/sway/issues/1318)

## Firmas digitales

Los lanzamientos son firmados con [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
y son publicados [en GitHub](https://github.com/swaywm/sway/releases).

## Estado de características

- [i3 - soporte de características](https://github.com/swaywm/sway/issues/2)
- [IPC - soporte de características](https://github.com/swaywm/sway/issues/98)
- [i3bar - soporte de características](https://github.com/swaywm/sway/issues/343)
- [i3-gaps - soporte de características](https://github.com/swaywm/sway/issues/307)
- [características de seguridad](https://github.com/swaywm/sway/issues/984)

## Instalación

### Por medio de paquetes

Sway esta disponible en muchas distribuciones. Intenta instalar el paquete
"sway" en tu distribución. Si no esta disponible revisa
[ésta wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
para información de la instalación en tu distribución.

Si estas interesado en crear un paquete de Sway para tu distribución entra 
al canal de IRC o envía un correo(en Inglés) a sir@cmpwn.com para más
información.

### Compilando código fuente

Instalación de dependencias:

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
* imagemagick (requerido para captura de imágenes con swaybrab)
* ffmpeg (requerido para captura de video con swaygrab)

_\*Sólo requerido para swaybar, swaybg, and swaylock_

_\*\*Sólo requerido para swaylock_

Ejecuta estos comandos:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

En sistemas con logind, se necesita fijar algunos "setcap" en el binario:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/bin/sway

En sistemas sin logind, se necesita habilitar el "suid" en el binario de sway:

    sudo chmod a+s /usr/local/bin/sway

## Configuración

Si ya haces uso de i3 sólo copia tu configuracirión de i3 a
`~/.config/sway/config` y sway funcionará sin configuraciones adicionales.
De lo contrario, copia el archivo de configuración de ejemplo a
`~/.config/sway/config`. El archivo de configuración generalmente se encuentra
en `/etc/sway/config`.
Ejecuta `man 5 sway` para mayor información en la configuración.

## Ejecución

Ejecuta `sway` desde una TTY. Algunos gestores de pantalla "display managers"
pueden ejecutar Sway, no todos soportan Sway (gdm tiende a funcionar bastante
bien)
