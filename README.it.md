# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - **[Italiano][it]** - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway è un compositore di [Wayland] compatibile con [i3]. Leggi le [FAQ].
Unisciti al [canale di IRC][IRC channel] \(#sway su irc.libera.chat).

## Firma delle versioni

Le versioni sono firmate con la chiave [E88F5E48] e sono pubblicate
[su GitHub][GitHub releases].

## Installazione

### Da un pacchetto

Sway è disponibile in molte distribuzioni, prova a installare il pacchetto
"sway" per la tua.

### Compilazione dei sorgenti

Consulta [questa pagina del wiki][Development setup] se vuoi compilare l'HEAD
di sway e wlroots per testarli o contribuire allo sviluppo.

Installa le dipendenze:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (opzionale: area di notifica)
* [scdoc] (opzionale: pagine del manuale) \*
* git (opzionale: informazioni sulla versione) \*

_\* Dipendenza necessaria per la compilazione_

Esegui questi comandi:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

Nei sistemi in cui non sono disponibili né logind né seatd, è necessario
impostare il permesso suid al binario di sway:

    sudo chmod a+s /usr/local/bin/sway

Sway rinuncerà ai permessi di root poco dopo l'avvio.

## Configurazione

Se hai già usato i3, copia il tuo file di configurazione in
`~/.config/sway/config` e sway funzionerà immediatamente. Altrimenti, copia il
file d'esempio in `~/.config/sway/config`, generalmente è situato in
`/etc/sway/config`. Consulta `man 5 sway` per ulteriori informazioni sulla
configurazione.

## Esecuzione

Lancia `sway` da un TTY. Alcuni gestori d'accesso potrebbero funzionare ma non
sono supportati da sway (gdm funziona abbastanza bene).

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
