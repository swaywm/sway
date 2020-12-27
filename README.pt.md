# sway

O sway é um compositor do [Wayland](http://wayland.freedesktop.org/) compatível com o [i3](https://i3wm.org/).
Leia o [FAQ](https://github.com/swaywm/sway/wiki). Junte-se ao [canal do    
IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway em
irc.freenode.net).

## Assinatura das versões

As versões são assinadas com [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
e publicadas [no GitHub](https://github.com/swaywm/sway/releases).

## Instalação

### A partir de pacotes
O Sway está disponível em várias distribuições. Tente instalar o pacote "sway"
na sua. Caso não esteja disponível, verifique [esta wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
para se informar a sobre a instalação para sua distribuição.

Se você está interessado em criar um pacote do sway para a sua distribuição, verifique canal do IRC
ou mande um email para sir@cmpwn.com para obter informações.

### Compilando a partir do código-fonte

Verifique [essa página da wiki](https://github.com/swaywm/sway/wiki/Development-Setup) se você quer compilar o HEAD do sway e o wlroots para testes ou desenvolvimento.

Instale as dependências:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (opcional: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (opcional: man pages) \*
* git (opcional: informações de versão) \*

_\*Dependência de tempo de compilação_

Execute esses comandos:

    meson build
    ninja -C build
    sudo ninja -C build install

Em sistemas sem logind, você precisa preparar o binário do sway:

    sudo chmod a+s /usr/local/bin/sway

O sway perderá as privilégios de de root logo após o início do sistema.

## Configuração

Se você já utiliza o i3, então copie os seus arquivos de configuração para `~/.config/sway/config` e
tudo funcionará normalmente. Caso contrário, copie o arquivo de configuração de exemplo para
`~/.config/sway/config`. Normalmente, este arquivo está localizado em `/etc/sway/config`.
Execute `man 5 sway` para se informar sobre a configuração.

## Execução

Execute o comando `sway` de um TTY. Alguns gerenciadores de display (ou gerenciadores de login) podem funcionar mas alguns não são suportados
pelo sway (o gdm é conhecido por funcionar bem).
