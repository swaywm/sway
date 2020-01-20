# sway

O sway é um compositor do [Wayland](http://wayland.freedesktop.org/) compatível com o [i3](https://i3wm.org/).
<!-- sway is an [i3](https://i3wm.org/)-compatible [Wayland](http://wayland.freedesktop.org/) compositor. -->
Leia o [FAQ](https://github.com/swaywm/sway/wiki). Junte-se ao [canal do    
IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway em
irc.freenode.net).

Se você gostaria de apoiar o desenvolvimento do sway, por favor, contribua na [página do patreon de
SirCmpwn](https://patreon.com/sircmpwn).

## Assinatura das versões
As versões são assinadas com [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
e publicadas [no GitHub](https://github.com/swaywm/sway/releases).
<!-- Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/swaywm/sway/releases). -->

## Instalação

### A partir de pacotes
O Sway está disponível em várias distribuições. Tente instalar o pacote "sway"
na sua. Caso não esteja disponível, verifique [esta wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
para se informar a sobre a instalação para sua distribuição.

<!-- Sway is available in many distributions. Try installing the "sway" package for
yours. If it's not available, check out [this wiki page](https://github.com/swaywm/sway/wiki/Unsupported-packages)
for information on installation for your distributions. -->
Se você está interessado em criar um pacote do sway para a sua distribuição, verifique canal do IRC
ou mande um email para sir@cmpwn.com para obter informações.

<!-- If you're interested in packaging sway for your distribution, stop by the IRC
channel or shoot an email to sir@cmpwn.com for advice. -->
### Compilando a partir do código-fonte

Instale as dependências:
<!-- Install dependencies: -->

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (Opcional: man pages) \*
* git \*

_\*Dependência de tempo de compilação_
<!-- _\*Compile-time dep_ -->

Rode esses comandos:
<!-- Run these commands: -->

    meson build
    ninja -C build
    sudo ninja -C build install

Em sistemas sem logind, você precisa preparar o binário do sway:
<!-- On systems without logind, you need to suid the sway binary: -->

    sudo chmod a+s /usr/local/bin/sway

O sway perderá as privilégios de de root logo após o início do sistema.
<!-- Sway will drop root permissions shortly after startup. -->

## Configuração

Se você já utiliza o i3, então copie os seus arquivos de configuração para `~/.config/sway/config` e
tudo funcionará normalmente. Caso contrário, copie o arquivo de configuração de exemplo para
`~/.config/sway/config`. Normalmente, este arquivo está localizado em `/etc/sway/config`.
Execute `man 5 sway` para se informar sobre a configuração.

<!-- If you already use i3, then copy your i3 config to `~/.config/sway/config` and
it'll work out of the box. Otherwise, copy the sample configuration file to
`~/.config/sway/config`. It is usually located at `/etc/sway/config`.
Run `man 5 sway` for information on the configuration. -->
## Execução

Execute o comando `sway` de um TTY. Alguns gerenciadores de diplay(ou gerenciadores de login) podem funcionar mas alguns não são suportaods
pelo sway (o gdm é conhecido por funcionar bem). 
<!-- Run `sway` from a TTY. Some display managers may work but are not supported by
sway (gdm is known to work fairly well). -->
