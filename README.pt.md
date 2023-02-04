# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - **[Português][pt]** - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

O sway é um compositor do [Wayland] compatível com o [i3].
Leia o [FAQ]. Junte-se ao [canal do    
IRC][IRC channel] (#sway em
irc.libera.chat).

## Assinatura das versões

As versões são assinadas com [E88F5E48]
e publicadas [no GitHub][Github releases].

## Instalação

### A partir de pacotes
O Sway está disponível em várias distribuições. Tente instalar o pacote "sway"
na sua. Caso não esteja disponível, verifique [esta wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
para se informar a sobre a instalação para sua distribuição.

Se você está interessado em criar um pacote do sway para a sua distribuição, verifique canal do IRC
ou mande um email para sir@cmpwn.com para obter informações.

### Compilando a partir do código-fonte

Verifique [essa página da wiki][Development setup] se você quer compilar o HEAD do sway e o wlroots para testes ou desenvolvimento.

Instale as dependências:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (opcional: system tray)
* [scdoc] (opcional: man pages) \*
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

[en]: https://github.com/swaywm/sway#readme
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[gr]: README.gr.md
[hi]: README.hi.md
[hu]: README.hu.md
[ir]: README.ir.md
[it]: README.it.md
[ja]: README.ja.md
[ko]: README.ko.md
[nl]: README.nl.md
[no]: README.no.md
[pl]: README.pl.md
[pt]: README.pt.md
[ro]: README.ro.md
[ru]: README.ru.md
[sv]: README.sv.md
[tr]: README.tr.md
[uk]: README.uk.md
[zh-CN]: README.zh-CN.md
[zh-TW]: README.zh-TW.md
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
