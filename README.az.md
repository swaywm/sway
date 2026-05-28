# sway

sway [i3]-ə uyğun [Wayland] kompozitorudur. [Tez-tez verilən sualları] oxuyun. 
[IRC kanalına] qoşulun ("irc.libera.chat"-da #sway).

## Buraxılış İmzaları

Buraxılışlar [E88F5E48] ilə imzalanıb və [GitHub-da][GitHub releases] dərc edilib.

## Quraşdırma

### Paketlərdən

Sway bir çox distributivdə mövcuddur. Öz distributiviniz üçün 
"sway" paketini quraşdırmağa çalışın.

### Mənbə kodundan kompilyasiya

Test və ya inkişaf üçün sway və wlroots-un HEAD-ini qurmaq istəyirsinizsə, 
[bu viki səhifəsini][Development setup] nəzərdən keçirin.

Asılılıqları quraşdırın:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (ixtiyari: sistem trayı üçün əlavə şəkil formatları)
* [swaybg] (ixtiyari: divar kağızı)
* [scdoc] (ixtiyari: man səhifələri) \*
* git (ixtiyari: versiya məlumatı) \*

_\* Kompilyasiya asılılıqları_

Bu əmrləri icra edin:

    meson setup build/
    ninja -C build/
    sudo ninja -C build/ install

## Konfiqurasiya

Əgər artıq i3-dən istifadə edirsinizsə, i3 konfiqurasiyanızı `~/.config/sway/config` 
ünvanına köçürün və o, dərhal işləyəcək. Əks halda, nümunə konfiqurasiya faylını 
`~/.config/sway/config` ünvanına köçürün. O, adətən `/etc/sway/config` ünvanında yerləşir.
Konfiqurasiya haqqında məlumat üçün `man 5 sway` əmrini icra edin.

## İşə Salma

TTY-dan `sway`-ı işə salın. Bəzi ekran menecerləri işləyə bilər, lakin sway tərəfindən 
dəstəklənmir (gdm-in kifayət qədər yaxşı işlədiyi məlumdur).

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[Tez-tez verilən sualları]: https://github.com/swaywm/sway/wiki
[IRC kanalına]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
