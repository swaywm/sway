# sway


Sway, [i3] uyumlu bir [Wayland] dizgicisidir. [SSS][FAQ]'yi okuyun. 
[IRC kanalı][IRC channel]na katılın \(irc.libera.chat'te #sway (İngilizce)).

## Sürüm imzaları

Sürümler [E88F5E48] ile imzalanır ve [GitHub][GitHub releases]'da yayımlanır.

## Kurulum

### Paketler ile

Sway birçok dağıtımda mevcuttur. Sizinki için "sway" paketini yüklemeyi deneyin.

Dağıtımınız için sway'i paketlemekle ilgileniyorsanız, IRC kanalına uğrayın veya tavsiye için sir@cmpwn.com adresine bir e-posta gönderin.

### Kaynak koddan derleme

Test veya geliştirme için sway ve wlroots'un HEAD'ini oluşturmak istiyorsanız [bu wiki sayfası][Development setup]na göz atın.

Aşağıdaki bağımlılıkları yükleyin:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (isteğe bağlı: sistem tepsisi)
* [scdoc] (isteğe bağlı: man sayfaları) \*
* git (isteğe bağlı: sürüm bilgisi) \*

_\*Derleme aşaması bağımlılıkları_

Şu komutları çalıştırın:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

logind ya da seatd olmayan sistemlerde, sway ikilisine suid etmeniz (yetkilendirmeniz) gerekir:

    sudo chmod a+s /usr/local/bin/sway

Sway, başlangıçtan kısa bir süre sonra kök izinlerini bırakacaktır.

## Yapılandırma

Halihazırda i3 kullanıyorsanız, i3 yapılandırmanızı `~/.config/sway/config` konumuna kopyalayın ve olduğu gibi çalışacaktır. Aksi takdirde, örnek yapılandırma dosyasını `~/.config/sway/config` konumuna kopyalayın. Genellikle `/etc/sway/config` konumunda bulunur.
Yapılandırma hakkında bilgi almak için `man 5 sway` komutunu çalıştırın.

## Çalıştırma

Bir TTY'den `sway` çalıştırın. Bazı giriş ekranı yöneticileriyle çalışabilir ama Sway tarafından desteklenmez. (gdm'nin oldukça iyi çalıştığı bilinmektedir.)

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
