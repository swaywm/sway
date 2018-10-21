# sway

[English](https://github.com/swaywm/sway/blob/master/README.md#sway--) -
[日本語](https://github.com/swaywm/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/swaywm/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/swaywm/sway/blob/master/README.el.md#sway--) - [Français](https://github.com/swaywm/sway/blob/master/README.fr.md#sway--) - [Українська](https://github.com/swaywm/sway/blob/master/README.uk.md#sway--) - [Italiano](https://github.com/swaywm/sway/blob/master/README.it.md#sway--) - [**Português**](https://github.com/swaywm/sway/blob/master/README.pt.md#sway--) -
[Русский](https://github.com/swaywm/sway/blob/master/README.ru.md#sway--) - [Български](https://github.com/swaywm/sway/blob/master/README.bg.md#sway--)

"**S**irCmpwn's **Way**land compositor" é um compositor [Wayland][], **em desenvolvimento**,
compatível com o gerenciador de janelas [i3][].  Leia o [FAQ][sway faq].
Participe do [canal no IRC][sway irc] (#sway em irc.freenode.net).

Se desejas apoiar o desenvolvimento do projeto, podes contribuir com
SirCmpwn em sua [página no Patreon][sircmpwn patreon].

## Ajuda em português

No momento, o suporte em português no canal no IRC **não está ativo**.
Caso haja algum problema, abra uma [*issue*][sway issues]
(*em inglês*).

A tradução em português é um **trabalho em progresso**. Caso encontre
erros ou queira colaborar com a tradução, visite [essa *issue*][sway i18n]
para mais informações e não hesite em enviar quaisquer correções
necessárias.

## Assinaturas dos lançamentos

Cada versão é assinada com a chave [B22DA89A][sway pgp] e publicada
[no Github][sway releases].

## Instalação

### A partir de pacotes

Sway está disponível em várias distribuições. Verifique se o pacote
"sway" está disponível para ser instalado na sua distribuição GNU/Linux. Caso não esteja,
procure por informações sobre como instalar Sway na sua distribuição
[aqui][sway pkg wiki].

Se estiveres interessado em manter um pacote em sua distribuição, visite o
canal no IRC ou mande um e-mail para <sir@cmpwn.com> (*em inglês*).

### A partir do código-fonte

Instale as seguintes dependências:

* meson *
* [wlroots][]
* wayland
* wayland-protocols *
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 **
* pam (opcional: Para suporte PAM  para o swaylock)
* [scdoc][] (opcional: Para as páginas de manuais)
* git *

_\* Dependências em tempo de compilação

_\*\*opcional: Necessário apenas para swaybg, e swaylock_

Execute os seguintes comandos:

    meson build
    ninja -C build
    sudo ninja -C build install

Em sistemas sem logind, você precisa ativar as *flags* de suid no executável gerado:

    sudo chmod a+s /usr/local/bin/sway

Sway removerá as permissões de root logo após o início do processo.

## Configuração

Se já usas i3, copie teu arquivo de configuração para `~/.config/sway/config`
e Sway lerá o arquivo normalmente.  Senão, copie o arquivo de configuração
de exemplo para `~/.config/sway/config`. É comum esse arquivo estar localizado
em `/etc/sway/config`. Veja `man 5 sway` para informações sobre configuração.

## Executando

Execute `sway` a partir do console. Alguns gerenciadores de *display* podem
funcionar, porém não são oficialmente suportados (segundo relatos, gdm
funciona bem com Sway).

[i3]:               https://i3wm.org/
[scdoc]:            https://git.sr.ht/~sircmpwn/scdoc
[sircmpwn patreon]: https://patreon.com/sircmpwn
[sway faq]:         https://github.com/swaywm/sway/wiki
[sway i18n]:        https://github.com/swaywm/sway/issues/1318
[sway irc]:         http://webchat.freenode.net/?channels=sway&uio=d4
[sway issues]:      https://github.com/swaywm/sway/issues
[sway pgp]:         http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A
[sway pkg wiki]:    https://github.com/swaywm/sway/wiki/Unsupported-packages
[sway releases]:    https://github.com/swaywm/sway/releases
[wayland]:          http://wayland.freedesktop.org/
[wlroots]:          https://github.com/swaywm/wlroots
