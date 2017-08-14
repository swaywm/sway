# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Doná con fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor" es un compositor para
[Wayland](http://wayland.freedesktop.org/) **en progreso** compatible con i3.

Leé las [preguntas frequentes](https://github.com/SirCmpwn/sway/wiki). Entrá al
[canal de IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway en
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Si te gustaría contribuir el desarrollo de Sway, podés colaborar en [mi página
de Patreon](https://patreon.com/sircmpwn) o podés contribuir a las
[recompensas](https://github.com/SirCmpwn/sway/issues/986) para funcionalidades
específicas.
Cualquiera es bienvenido a reclamar una recompensa, y podés ofrecer una
recompensar por una funcionalidad que quieras. Patreon es más útil para
auspiciar la salud general del proyecto y el mantenimiento de Sway.

## Ayuda en español

@hobarrera provee soporte en español en GitHub.

Esta traducción está en progreso, ver
[acá](https://github.com/SirCmpwn/sway/issues/1318) para maś detalle.

## Firmas de releases

Los releases son firmados con [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
y publicados [en GitHub](https://github.com/SirCmpwn/sway/releases).

## Estado

- [soporte de funcionalidades de i3](https://github.com/SirCmpwn/sway/issues/2)
- [soporte de funcionalidades de IPC](https://github.com/SirCmpwn/sway/issues/98)
- [soporte de funcionalidades de i3bar](https://github.com/SirCmpwn/sway/issues/343)
- [soporte de funcionalidades de i3-gaps](https://github.com/SirCmpwn/sway/issues/307)
- [funcionalidades de seguridad](https://github.com/SirCmpwn/sway/issues/984)

## Instalación

### Por paquetes

Sway está disponible en muchas distribuciones. Probá instalando el paquete
"sway" para la tuya. Si no está disponible, revisá [esta página de la
wiki](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages) para
información de como instalar para tu distribución.

Si estás interesado en crear paquetes de Sway para tu distribución, pasá por el
canal de IRC, o mandále un mail a sir@cmpwn.com [en inglés] para consejos.

### Compilar desde el código

Instalá dependencias:

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
* imagemagick (requerido para capturar imágenes con swaygrab)
* ffmpeg (requerido para capturar video con swaygrab)

_\*Sólo requerido para swaybar, swaybg, y swaylock_

_\*\*Sólo requerido para swaylock_

Corré estos comandos:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

En sistemas con logind, necesitás setearle unas capacidades al binario:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

En systemas sin logind, necesitás setearle suid al binario de sway:

    sudo chmod a+s /usr/local/bin/sway

## Configuración

Si ya usás i3, entonces copiá tu configuración de i3 a `~/.config/sway/config`
y te va a andar de una. Sino, copiá la configuración de muestra a 
`~/.config/sway/config`. Generalmente está ubicada en `/etc/sway/config`.
Consultá `man 5 sway` para más información sobre la configuración.

Mis propios archivos de configuración están disponibles
[acá](https://git.sr.ht/~sircmpwn/dotfiles) si querés algo de inspiración, y
definitivamente también consultá [la wiki](https://github.com/SirCmpwn/sway/wiki).

## Ejecución

En vez de correr `startx`, corré `sway`. Podés correr `sway` desde dentro de X,
lo cual es útil para testeo.
