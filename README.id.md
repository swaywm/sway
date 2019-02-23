# sway

[**English**](https://github.com/swaywm/sway/blob/master/README.md#sway--) - [日本語](https://github.com/swaywm/sway/blob/master/README.ja.md#sway--) - [Deutsch](https://github.com/swaywm/sway/blob/master/README.de.md#sway--) - [Ελληνικά](https://github.com/swaywm/sway/blob/master/README.el.md#sway--) - [Français](https://github.com/swaywm/sway/blob/master/README.fr.md#sway--) - [Українська](https://github.com/swaywm/sway/blob/master/README.uk.md#sway--) - [Indonesian](https://github.com/swaywm/sway/blob/master/README.id.md#sway--) - [Italiano](https://github.com/swaywm/sway/blob/master/README.it.md#sway--) - [Português](https://github.com/swaywm/sway/blob/master/README.pt.md#sway--) -
[Русский](https://github.com/swaywm/sway/blob/master/README.ru.md#sway--) - [Български](https://github.com/swaywm/sway/blob/master/README.bg.md#sway--)

sway adalah compositor untuk i3 [Wayland](http://wayland.freedesktop.org/).
Untuk pertanyaan dan jawaban ikut IRCnya [FAQ](https://github.com/swaywm/sway/wiki). Ikut [channel
 IRC](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net).

Kalau mau mendukung proyek ini, bikin kontribusi di Patreon [Halaman Pateron SirCmpwn](https://patreon.com/sircmpwn).

## Signature Rilis

Semua rilis tertanda dengan B22DA89A [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
dan diterbitkan [di GitHub](https://github.com/swaywm/sway/releases).

## Instalasi

### Dari Packages

Sway tersedia dalam beberapa distribusi. Lakukan instalasi dengan package "sway" terlebih dahulu. 
Apabila gagal, lihatlah [halaman wiki](https://github.com/swaywm/sway/wiki/Unsupported-packages)
untuk informasi instalasi untuk tiap distribusi.

Jika anda tertarik membuat package untuk distribusi anda, kunjungi channel IRC
atau kirim email ke sir@cmpwn.com untuk saran.

### Kompilasi dari Sumber

Ketergantungan dalam instaasi:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 \*\*
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) >= 1.8.1 (optional: man pages) \*
* git \*

_\*Ketergantungan kompilasi

_\*\*Tidak wajib: hanya diperlukan untuk swaybg_

Jalankan commands dibawah:

    meson build
    ninja -C build
    sudo ninja -C build install

Untuk sistem tanpa logind, pakai:

    sudo chmod a+s /usr/local/bin/sway

Sway akan membuang permisi root setelah startup.

## Konfigurasi

Jika anda sudah menggunakan i3, duplikatkan konfigurasi i3 anda ke `~/.config/sway/config` dan sway akan langsung siap. 
Jika anda belum menggunakan i3, duplikatkan contoh konfigurasi ke
`~/.config/sway/config`. Contoh dapat ditemukan di `/etc/sway/config`.
Paki `man 5 sway` untuk informasi mengenai konfigurasi.

## Menjalankan sway

Ketik `sway` dari TTY. beberapa display managers mungkin bekerja tetapi tidak didukung oleh
sway (gdm umumnya dapat bekerja dengan baik).
