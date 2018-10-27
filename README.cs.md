# sway

"**S**irCmpwn's **Way**land compositor" je v **procesu vývoje**
i3-kompatibilní [Wayland](http://wayland.freedesktop.org/) kompozitní správce.
Přečtěte si [často kladené dotazy](https://github.com/swaywm/sway/wiki). Připojte se na 
[IRC kanál](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway na
irc.freenode.net).

Pokud chcete podpořit vývoj kompozitního správce Sway, prosím přispějte na [Patreonu SirCmpwn](https://patreon.com/sircmpwn).

## Digitální podpis vydání

Vydání jsou podepsána pomocí [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
a publikována [na GitHubu](https://github.com/swaywm/sway/releases).

## Instalace

### Z balíčků

Sway je dostupný v mnoha distribucích. Zkuste nainstalovat "sway" balíček v
té vaší. Pokud není dostupný, více informací o instalaci ve vaší distribuci 
najdete na [této wiki stránce](https://github.com/swaywm/sway/wiki/Unsupported-packages).

Pokud máte zájem o vytvoření sway balíčku pro vaši distribuci, zastavte se na IRC
kanálu nebo si o radu napište na sir@cmpwn.com.

### Kompilace ze zdroje

Balíky potřebné k instalaci:

* meson
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* pcre
* json-c >= 0.13
* pango
* cairo
* gdk-pixbuf2 *
* pam **
* dbus >= 1.10 ***
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (požadováno pro man stránky)
* git

_\*Požadováno pouze pro swaybar, swaybg, and swaylock_

_\*\*Požadováno pouze pro swaylock_

_\*\*\*Požadováno pouze pro tray support_

Spusťte tyto příkazy:

    meson build     
    ninja -C build
    sudo ninja -C build install

Na systémech bez logind, je potřeba nastavit suid na binární soubor:

    sudo chmod a+s /usr/local/bin/sway

Sway krátce po spuštění ztratí root práva.

## Konfigurace

Pokud už používate i3, zkopírujte svůj i3 config do `~/.config/sway/config` a
sway bude fungovat. Pokud ne, zkopírujte soubor s ukázkovou konfigurací
`~/.config/sway/config`. Obvykle je umístěn v `/etc/sway/config`.
Pro více informací o konfiguraci spusťte `man 5 sway`.

## Spuštění

Spusťte `sway` z terminálu TTY. Některé display managery mohou fungovat, ale
sway je nepodporuje (gdm obvykle funguje celkem dobře).
