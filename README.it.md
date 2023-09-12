# sway

sway è un compositore di [Wayland] compatibile con [i3]. Leggi le [FAQ].
Unisciti al [canale di IRC] \(#sway su irc.libera.chat).

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

## Configurazione

Se hai già usato i3, copia il tuo file di configurazione in
`~/.config/sway/config` e sway funzionerà immediatamente. Altrimenti, copia il
file d'esempio in `~/.config/sway/config`, generalmente è situato in
`/etc/sway/config`. Consulta `man 5 sway` per ulteriori informazioni sulla
configurazione.

## Esecuzione

Lancia `sway` da un TTY. Alcuni gestori d'accesso potrebbero funzionare ma non
sono supportati da sway (gdm funziona abbastanza bene).

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[canale di IRC]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
