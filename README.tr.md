# sway


Sway, [i3] uyumlu bir [Wayland] dizgicisidir. [SSS][FAQ]'yi okuyun. 
[IRC kanalına][IRC channel] katılın \(irc.libera.chat'te #sway (İngilizce)).

## Sürüm imzaları

Sürümler [E88F5E48] ile imzalanır ve [GitHub][GitHub releases]'da yayınlanır.

## Kurulum

### Paketler ile

Sway birçok dağıtımda bulunmaktadır. Sizinki için “sway” paketini yüklemeyi deneyin.

### Kaynak koddan derleme

Denemek ya da geliştirmek için sway ve wlroots'un HEAD'ini derlemek istiyorsanız [bu wiki sayfasına][Development setup] göz atın.

Aşağıdaki bağımlılıkları yükleyin:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (isteğe bağlı: sistem tepsisi için ek görüntü biçimleri)
* [swaybg] (isteğe bağlı: Duvar kağıdı)
* [scdoc] (isteğe bağlı: man kullanma kılavuzu) \*
* git (isteğe bağlı: sürüm bilgisi) \*

_\*Derleme aşaması bağımlılıkları_

Şu komutları çalıştırın:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## Yapılandırma

Hâlihazırda i3 kullanıyorsanız, i3 yapılandırmanızı `~/.config/sway/config` konumuna kopyalayınca olduğu gibi çalışacaktır. Yoksa, örnek yapılandırma dosyasını yine `~/.config/sway/config` konumuna kopyalayın. Genellikle `/etc/sway/config` konumunda bulunur.
Yapılandırma hakkında bilgi almak için `man 5 sway` komutunu çalıştırın.

## Çalıştırma

TTY'den `sway` komutunu çalıştırın. Bazı görüntü yöneticileriyle çalışabilir ama sway tarafından desteklenmez. (gdm'nin oldukça iyi çalıştığı bilinmektedir.)

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
