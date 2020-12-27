# sway

Sway est un compositeur [Wayland](http://wayland.freedesktop.org/) compatible
avec [i3](https://i3wm.org/), **en cours de développement**.  Lisez la
[FAQ](https://github.com/swaywm/sway/wiki). Rejoignez le [canal
IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway sur
irc.freenode.net).

## Aide en français

[abdelq](//github.com/abdelq) fournit du support en français sur IRC et Github, dans le fuseau horaire UTC-4 (EST).

## Signatures de nouvelles versions

Les nouvelles versions sont signées avec [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
et publiées [sur GitHub](https://github.com/swaywm/sway/releases).

## Installation

### À partir de paquets

Sway est disponible sur plusieurs distributions. Essayez d'installer le paquet "sway" pour
la vôtre. Si ce n'est pas disponible, consultez [cette page wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
pour de l'information sur l'installation pour vos distributions.

Si vous êtes intéressé à maintenir Sway pour votre distribution, passez par le canal
IRC ou envoyez un e-mail à sir@cmpwn.com (en anglais seulement) pour des conseils.

### Compilation depuis la source

Installez les dépendances :

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (optionnel: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optionnel: requis pour les pages man) \*
* git \*

_\*Requis uniquement pour la compilation_

Exécutez ces commandes :

    meson build
    ninja -C build
    sudo ninja -C build install

Sur les systèmes sans logind, vous devez suid le binaire de sway :

    sudo chmod a+s /usr/local/bin/sway

Sway se débarassera des permissions *root* peu de temps après le démarrage.

## Configuration

Si vous utilisez déjà i3, copiez votre configuration i3 à `~/.config/sway/config` et
cela va fonctionner. Sinon, copiez l'exemple de fichier de configuration à
`~/.config/sway/config`. Il se trouve généralement dans `/etc/sway/config`.
Exécutez `man 5 sway` pour l'information sur la configuration.

## Exécution

Exécutez `sway` à partir d'un TTY. Certains gestionnaires d'affichage peuvent fonctionner,
mais ne sont pas supportés par Sway (gdm est réputé pour assez bien fonctionner).
