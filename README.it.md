# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor" è un compositor 
[Wayland](http://wayland.freedesktop.org/) **in via di sviluppo** 
compatibile con i3.
Leggi le [FAQ (in Inglese)](https://github.com/SirCmpwn/sway/wiki). Unisciti al
[canale IRC (in Inglese)](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Se vuoi supportare lo sviluppo di Sway, puoi contribuire dalla 
[pagina Patreon di SirCmpwn's](https://patreon.com/sircmpwn) o con dei
[premi](https://github.com/SirCmpwn/sway/issues/986) per finanziare lo sviluppo
di funzionalità specifiche.
Chiunque è libero di reclamare un premio o crearne uno per qualsiasi funzionalità.
Patreon è più utile al supporto e alla manutenzione generale di Sway.

## Supporto italiano
syknro offre supporto in Italiano su GitHub nel fuso orario CET/CEST.
Questa traduzione non è ancora completa. [Clicca qui per maggiori informazioni](https://github.com/SirCmpwn/sway/issues/1318)

## Firme digitali

Le release sono firmate con [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
e pubblicate [su GitHub](https://github.com/SirCmpwn/sway/releases).

## Status

- [supporto funzionalità i3](https://github.com/SirCmpwn/sway/issues/2)
- [supporto funzionalità IPC](https://github.com/SirCmpwn/sway/issues/98)
- [supporto funzionalità i3bar](https://github.com/SirCmpwn/sway/issues/343)
- [supporto funzionalità i3-gaps](https://github.com/SirCmpwn/sway/issues/307)
- [sicurezza](https://github.com/SirCmpwn/sway/issues/984)

## Installazione

### Dai pacchetti

Sway è disponibile in molte distribuzioni. Prova a installare il pacchetto "sway" per la tua.
Se non funziona, controlla [questa pagina (in Inglese)](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages)
per informazioni sull'installazione per le tue distribuzioni.

Se vuoi creare un pacchetto per la tua distribuzione, passa dall'IRC o manda un email (in Inglese)
a sir@cmpwn.com.

### Compilando il codice sorgente

Installa queste dipendenze:

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
* imagemagick (richiesto per catturare immagini con swaygrab)
* ffmpeg (rrichiesto per catturare video con swaygrab)

_\*Richiesto solo per swaybar, swaybg, e swaylock_

_\*\*Richiesto solo per swaylock_

Esegui questi comandi:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

Per i sistemi con logind, devi impostare un paio di caps sull'eseguibile:

    sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
    sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

Per i sistemi senza logind, devi cambiare i permessi (suid):

    sudo chmod a+s /usr/local/bin/sway

## Configurazione

Se usi i3, copia la tua configurazione in `~/.config/sway/config` e
funzionerà direttamente. 
Altrimenti copia in `~/.config/sway/config` la configurazione di esempio, 
solitamente si trova in `/etc/sway/config`.
Esegui `man 5 sway` per informazioni sulla configurazione.

## Esecuzione

Esegui `sway` da un TTY. Qualche display manager potrebbe funzionare ma non sono
ufficialmente supportati da Sway (gdm è risaputo funzionare abbastanza bene).

## Supporto italiano
syknro offre supporto in Italiano su GitHub nel fuso orario CET/CEST.
Questa traduzione non è ancora completa. [Clicca qui per maggiori informazioni](https://github.com/SirCmpwn/sway/issues/1318)