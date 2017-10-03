# sway [![](https://api.travis-ci.org/swaywm/sway.svg)](https://travis-ci.org/swaywm/sway) [![Doe através do fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor" é um compositor [Wayland](http://wayland.freedesktop.org/)
compatível com o i3. Leia o [FAQ](https://github.com/swaywm/sway/wiki). Participe do
[canal IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway no
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Se você deseja apoiar o desenvolvimento do Sway, você pode contribuir com o
SirCmpwn em sua [página no Patreon](https://patreon.com/sircmpwn) ou você
pode colaborar com [premiações](https://github.com/swaywm/sway/issues/986)
para recursos específicos. Qualquer um pode requerer uma premiação ao implementar
o recurso especificado, e você pode criar uma premiação para qualquer recurso que desejar.
O Patreon é melhor direcionado para a manutenção a longo prazo do Sway.

## Ajuda em português

No momento, o suporte em português no canal do IRC **não está ativo**. Em caso de problemas,
use as [*issues*](https://github.com/swaywm/sway/issues/) do Github (*em inglês*).

A tradução para português é um *trabalho em progresso*, no momento. Caso encontre algum erro
ou queira colaborar com a tradução, visite
[essa *issue*](https://github.com/swaywm/sway/issues/1318) para mais informações e não
exite em enviar quaisquer correções necessárias.

## Assinaturas dos *Releases*

*Releases* são assinadas com a chave
[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
e publicadas [no GitHub](https://github.com/swaywm/sway/releases).

## Status

- [Suporte aos recursos do i3](https://github.com/swaywm/sway/issues/2)
- [Suporte aos recursos IPC](https://github.com/swaywm/sway/issues/98)
- [Suporte aos recursos do i3bar](https://github.com/swaywm/sway/issues/343)
- [Suporte aos recursos do i3-gaps](https://github.com/swaywm/sway/issues/307)
- [Recursos de segurança](https://github.com/swaywm/sway/issues/984)

## Instalação

### A partir de pacotes

Sway está disponível em várias distribuições. Verifique se o pacote "sway" está
disponível a partir do gerenciador de pacotes da sua distribuição. Caso não esteja,
procure por informações sobre como instalar o Sway na sua distribuição
[aqui](https://github.com/swaywm/sway/wiki/Unsupported-packages).

Se você está interessado em manter um pacote do Sway para a sua distribuição,
visite o canal no IRC ou mande um email para sir@cmpwn.com (*em inglês*).

### A partir do código-fonte

Antes de iniciar a compilação, instale as dependências:

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
* imagemagick (capturar imagem com o swaygrab)
* ffmpeg (capturar vídeo com o swaygrab)

_\*Dependência apenas de swaybar, swaybg, e swaylock_

_\*\*Dependência apenas de swaylock_

Para compilar, execute estes comandos:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

Em sistemas com logind, configure as seguintes capacidades para o arquivo binário:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/bin/sway

Em sistemas sem logind, ative a *flag* de *suid* do arquivo binário:

    sudo chmod a+s /usr/local/bin/sway

## Configuração

Se você já usa o i3, copie o arquivo de configuração do i3 para `~/.config/sway/config`;
o Sway lerá o arquivo normalmente. Senão, copie o arquivo de configuração de exemplo
para `~/.config/sway/config`. É comum esse arquivo estar localizado em
`/etc/sway/config`. Veja `man 5 sway` para informações sobre configuração.

## Executando

Execute `sway` a partir de um terminal do Linux. Alguns gerenciadores de *display*
podem funcionar, porém o Sway não procura manter compatibilidade com esses (segundo
relatos, o gdm funciona bem com o Sway).

