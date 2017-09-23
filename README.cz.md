# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

Stav českého překladu můžete zkontrolovat [zde](https://github.com/SirCmpwn/sway/issues/1318)

"**S**irCmpwn's **Way**land compositor" je **rozpracovaný** i3-kompatibilní
kompozitor pro [Wayland](http://wayland.freedesktop.org/). K dispozici jsou
**časté otázky a odpovědi** [FAQ](https://github.com/SirCmpwn/sway/wiki).
A diskusní [IRC kanál](http://webchat.freenode.net/?channels=sway&uio=d4)
(#sway na irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Pokud chcete podpořit vývoj sway, můžete přispět pomocí [SirCmpwn
Patreon stránky](https://patreon.com/sircmpwn) nebo můžete přispět
[odměnou](https://github.com/SirCmpwn/sway/issues/986) na konkrétní
rozšíření funkcí. Každý může získat odměnu za vytvoření žádané
funkcionality. A kdokoli může zadat odměnu na novou funkcionalitu
kterou by si přál. Pro obecnou podporu fungováni a údržbu projektu je
však vhodnější Patreon.

## Česká podpora

gjask(UTC+1) nabízí pomoc v češtině na githubu a IRC (jask)

## Podepsaný software

Všechna vydání jsou podepsaná klíčem [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
a vystavený [na GitHub](https://github.com/SirCmpwn/sway/releases).

## Stav vývoje

- [podpora i3](https://github.com/SirCmpwn/sway/issues/2)
- [podpora IPC](https://github.com/SirCmpwn/sway/issues/98)
- [podpora i3bar](https://github.com/SirCmpwn/sway/issues/343)
- [podpora i3-gaps](https://github.com/SirCmpwn/sway/issues/307)
- [bezpečnost](https://github.com/SirCmpwn/sway/issues/984)

## Instalace

### Z balíčku

Sway je k dispozici v mnoha distribucích. Zkuste nainstalovat "sway" v té
své. Pokud není k dispozici, více informací o instalaci najdete na [téhle wiki
stránce](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages).

Pokud by vás zajímalo balíčkování Sway pro vaší distribuci, zastavte se na IRC
kanálu nebo pošlete email na sir@cmpwn.com pro radu.

### Kompilování ze zdrojových souborů

Závislosti pro build:

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
* imagemagick (nutné pro print-screen pomocí swaygrab)
* ffmpeg (nutné pro video záznam obrazovky pomocí swaygrab)

_\*Nutné pouze pro swaybar, swaybg, and swaylock_

_\*\*Nutné pouze pro swaylock_

Spusťte tyto příkazy:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

Na systémech s logind je třeba nastavit zvláštní práva:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

Na systémech bez logind je třeba nastavit suid na binárku:

    sudo chmod a+s /usr/local/bin/sway

## Nastavení

Pokud už používate i3, stačí zkopírovat i3 konfigurační soubor
`~/.config/sway/config` který by měl fugnovat tak, jak je. Pokud ne,
zkopírujte vzorovou konfiguraci do `~/.config/sway/config`. Obvykle je
dostupna na `/etc/sway/config`. Spusťte `man 5 sway` pro více informací
o konfiguraci.

## Spuštění

Spusťte `sway` v TTY. Některé displej manažery mohou spolupracovat ale nejsou
podporovány Swayem (gdm funguje celkem slušně).
